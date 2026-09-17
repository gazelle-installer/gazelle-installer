#!/bin/bash
set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
AUR_DIR="aur"
MAIN_BRANCH="master"
GIT_REMOTE="origin"
GITHUB_REPO="gazelle-installer/gazelle-installer"
ANNOTATION=""

# Helper functions
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  Gazelle Installer Arch Release Script${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo
}

print_step() {
    echo -e "${GREEN}> $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}WARNING: $1${NC}"
}

print_error() {
    echo -e "${RED}ERROR: $1${NC}"
}

print_success() {
    echo -e "${GREEN}OK: $1${NC}"
}

# Validate version format (semantic versioning or YY.MM format)
validate_version() {
    local version=$1

    # Remove 'v' prefix if present for validation
    local clean_version=${version#v}

    # Check semantic version format (major.minor.patch), YY.MM, or YY.MMsuffix format
    if ! [[ $clean_version =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?[a-z]*$ ]]; then
        print_error "Invalid version format: $version"
        echo "Expected formats: 1.0.0, v1.0.0, YY.MM (like 26.01), or YY.MMsuffix (like 26.01arch)"
        exit 1
    fi

    echo "$version"
}

# Check if tag already exists locally or on the remote
tag_exists() {
    local version=$1

    if git tag -l | grep -q "^${version}$"; then
        return 0
    fi

    if git ls-remote --tags "$GIT_REMOTE" 2>/dev/null | grep -q "refs/tags/${version}$"; then
        return 0
    fi

    return 1
}

# Get latest numeric release tag for comparison (local or remote)
get_latest_tag() {
    {
        git tag -l
        git ls-remote --tags "$GIT_REMOTE" 2>/dev/null | awk '{sub("refs/tags/", "", $2); sub(/\^\{\}$/, "", $2); print $2}'
    } | sort -u | while read -r tag; do
        local clean_tag=${tag#v}
        if [[ $clean_tag =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?[a-zA-Z0-9]*$ ]]; then
            printf '%s\n' "$tag"
        fi
    done | sort -V | tail -n1
}

# Get the annotation from an existing local annotated tag, if available
get_tag_annotation() {
    local version=$1
    git for-each-ref --format='%(contents)' "refs/tags/${version}" 2>/dev/null || true
}

# Compare versions (returns 0 if new > old, 1 if new <= old)
compare_versions() {
    local new_version=$1
    local old_version=$2

    # Remove 'v' prefix for comparison
    new_version=${new_version#v}
    old_version=${old_version#v}

    # Use sort -V for semantic version comparison
    if [ "$new_version" = "$(echo -e "$new_version\n$old_version" | sort -V | tail -n1)" ] && [ "$new_version" != "$old_version" ]; then
        return 0  # new > old
    else
        return 1  # new <= old
    fi
}

# Prompt user for annotation
prompt_annotation() {
    local version=$1

    echo
    print_step "Enter release annotation/notes"

    local annotation=""
    local tmpfile
    tmpfile=$(mktemp) || {
        print_error "Failed to create temp file"
        exit 1
    }
    cat > "$tmpfile" <<EOF
## Release $version

- Feature 1
- Bug fix 2
- Other changes
EOF

    if [ -n "${EDITOR:-}" ]; then
        "${EDITOR}" "$tmpfile"
    else
        echo "No EDITOR set. Enter a single-line annotation and press Enter:"
        echo "(Multi-line notes require setting EDITOR.)"
        read -r annotation
        if [ -n "$annotation" ]; then
            printf "%s\n" "$annotation" > "$tmpfile"
        fi
    fi

    annotation=$(sed '/^[[:space:]]*$/d' "$tmpfile")
    rm -f "$tmpfile"

    if [ -z "$annotation" ]; then
        print_error "Annotation cannot be empty"
        exit 1
    fi

    ANNOTATION="$annotation"
}

# Create annotated tag
create_tag() {
    local version=$1
    local annotation=$2

    print_step "Creating annotated tag '$version'..."

    # Create annotated tag
    git tag -a "$version" -m "$annotation"

    print_success "Tag '$version' created successfully"
    echo
    git show "$version" --stat
}

# Update AUR package
update_aur_package() {
    local version=$1
    local annotation=$2

    print_step "Updating AUR package..."

    # Check if aur directory exists
    if [ ! -d "$AUR_DIR" ]; then
        print_error "AUR directory '$AUR_DIR' not found"
        exit 1
    fi

    # Change to aur directory
    cd "$AUR_DIR"

    # Update PKGBUILD pkgver to match tag and remove pkgver() if present
    if [ -f PKGBUILD ]; then
        print_step "Updating PKGBUILD pkgver to $version..."
        if grep -q "^pkgver=" PKGBUILD; then
            sed -i "s/^pkgver=.*/pkgver=${version}/" PKGBUILD
        else
            awk -v ver="$version" '
                /^pkgname=/ { print; print "pkgver=" ver; next }
                { print }
            ' PKGBUILD > PKGBUILD.tmp && mv PKGBUILD.tmp PKGBUILD
        fi

        if grep -q "^pkgver()" PKGBUILD; then
            awk '
                BEGIN { in_pkgver = 0 }
                /^pkgver\(\)/ { in_pkgver = 1; next }
                in_pkgver && /^}/ { in_pkgver = 0; next }
                !in_pkgver { print }
            ' PKGBUILD > PKGBUILD.tmp && mv PKGBUILD.tmp PKGBUILD
        fi
    else
        print_error "PKGBUILD not found in $AUR_DIR"
        exit 1
    fi

    # Convert to tarball source and calculate checksum
    print_step "Converting to tarball source and calculating checksum..."
    local tarball_url="https://github.com/${GITHUB_REPO}/archive/refs/tags/${version}.tar.gz"

    # Update source in PKGBUILD
    sed -i "s|source=.*|source=(\"${tarball_url}\")|" PKGBUILD

    # Remove 'git' from makedepends without dropping the rest of the line
    sed -i "s/'git' //;s/ 'git'//;s/'git'//" PKGBUILD

    # Download tarball and calculate checksum (with retry)
    local checksum=""
    local retries=5
    for i in $(seq 1 $retries); do
        print_step "Attempting to download tarball (attempt $i/$retries)..."
        if curl -L --fail --silent --show-error "$tarball_url" -o "/tmp/${version}.tar.gz" 2>/dev/null; then
            checksum=$(sha256sum "/tmp/${version}.tar.gz" | cut -d' ' -f1)
            if [ -n "$checksum" ]; then
                print_success "Checksum calculated: ${checksum:0:16}..."
                break
            fi
        fi

        if [ $i -lt $retries ]; then
            print_warning "Failed to download tarball, waiting 5 seconds before retry..."
            sleep 5
        fi
    done

    if [ -z "$checksum" ]; then
        print_error "Failed to download tarball after $retries attempts"
        print_warning "You may need to calculate checksum manually later"
        checksum="PLACEHOLDER_NEEDS_ACTUAL_CHECKSUM"
    fi

    # Update checksum in PKGBUILD
    sed -i "s/sha256sums=.*/sha256sums=('${checksum}')/" PKGBUILD

    # Regenerate .SRCINFO from PKGBUILD
    print_step "Regenerating .SRCINFO..."
    makepkg --printsrcinfo > .SRCINFO

    # Check if there are any changes to commit
    print_step "Checking for AUR package changes..."

    if git diff --quiet && git diff --staged --quiet; then
        print_warning "No changes in AUR package - skipping commit"
    else
        print_step "Committing AUR package changes..."

        # Add all changes
        git add .

        # Commit changes
        git commit -m "$annotation"

        print_success "AUR package updated and committed"
        echo
        git show --stat HEAD
    fi

    # Clean up downloaded tarball
    rm -f "/tmp/${version}.tar.gz"

    # Go back to original directory
    cd ..
}

# Show manual push instructions
show_push_instructions() {
    local version=$1
    local tag_status=${2:-created}

    echo
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  MANUAL PUSH REQUIRED${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo
    print_warning "Please run this command manually:"
    echo
    echo "# Push AUR package update:"
    echo -e "${YELLOW}cd aur && git push${NC}"
    echo
    if [ "$tag_status" = "created" ]; then
        print_step "The tag has been created and pushed automatically."
    else
        print_step "The existing tag '$version' was reused; no tag was created."
    fi
    print_step "After pushing the AUR changes, the package will be ready for AUR submission."
}

# Main script
main() {
    local version=""
    local tag_status="created"

    # Parse arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            --update|--force)
                print_warning "$1 is no longer needed; existing tags are handled automatically"
                ;;
            --*)
                print_error "Unknown option: $1"
                exit 1
                ;;
            *)
                if [ -n "$version" ]; then
                    print_error "Only one version may be specified"
                    exit 1
                fi
                version=$1
                ;;
        esac
        shift
    done

    print_header

    # Check if we're in a git repository
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        print_error "Not in a git repository"
        exit 1
    fi

    # With no version given, target the most recent release tag
    if [ -z "$version" ]; then
        version=$(get_latest_tag)
        if [ -z "$version" ]; then
            print_error "No version tag found; specify a version explicitly"
            exit 1
        fi
        print_step "No version specified; using latest tag '$version'"
    fi

    # Validate and normalize version
    print_step "Validating version format..."
    version=$(validate_version "$version")
    print_success "Version format valid: $version"

    # Check if on main branch
    local current_branch=$(git branch --show-current)
    if [ "$current_branch" != "$MAIN_BRANCH" ]; then
        print_warning "Not on $MAIN_BRANCH branch (currently on: $current_branch)"
        echo "Continue anyway? (y/N)"
        read -r response
        if [[ ! "$response" =~ ^[Yy]$ ]]; then
            print_step "Aborted by user"
            exit 0
        fi
    fi

    # Reuse an existing tag instead of failing or attempting to recreate it
    print_step "Checking if tag '$version' already exists..."
    if tag_exists "$version"; then
        print_warning "Tag '$version' already exists; no new tag will be created"
        tag_status="existing"
        ANNOTATION=$(get_tag_annotation "$version")
        if [ -z "$ANNOTATION" ]; then
            ANNOTATION="Update AUR package to $version"
        fi
    else
        print_success "Tag '$version' is available"

        # Compare with latest tag only when creating a new tag
        print_step "Checking version progression..."
        local latest_tag=$(get_latest_tag)
        local clean_version=${version#v}

        if [ -n "$latest_tag" ] && ! compare_versions "$clean_version" "$latest_tag"; then
            print_error "Version $clean_version is not higher than latest tag ${latest_tag#v}"
            exit 1
        fi
        if [ -n "$latest_tag" ]; then
            print_success "Version $version > ${latest_tag#v}"
        fi

        # Prompt for annotation
        prompt_annotation "$version"
    fi

    local annotation="$ANNOTATION"

    if [ "$tag_status" = "created" ]; then
        # Confirm before proceeding
        echo
        print_warning "About to create tag '$version' with annotation:"
        echo "$annotation"
        echo
        echo "Continue? (y/N)"
        read -r response
        if [[ ! "$response" =~ ^[Yy]$ ]]; then
            print_step "Aborted by user"
            exit 0
        fi

        # Create and push the tag (needed for checksum calculation)
        create_tag "$version" "$annotation"

        print_step "Pushing tag to remote '$GIT_REMOTE'..."
        git push "$GIT_REMOTE" "$version"
        print_success "Tag pushed to GitHub"
    fi

    # Update AUR package (now with real checksum)
    update_aur_package "$version" "$annotation"

    # Show manual push instructions (only AUR now)
    show_push_instructions "$version" "$tag_status"

    echo
    print_success "Release preparation complete!"
    print_step "Don't forget to push the changes manually"
}

# Run main function with all arguments
main "$@"
