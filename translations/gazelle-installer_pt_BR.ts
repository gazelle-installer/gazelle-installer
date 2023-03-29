<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="pt_BR">
<context>
    <name>Base</name>
    <message>
        <location filename="../base.cpp" line="110"/>
        <source>Checking installation media.</source>
        <translation>Verificando a mídia de instalação.</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="111"/>
        <source>Press ESC to skip.</source>
        <translation>Pressione a tecla ESC para não continuar com a verificação da mídia de instalação.</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="119"/>
        <source>The installation media is corrupt.</source>
        <translation>A mídia de instalação está danificada.</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="174"/>
        <source>Are you sure you want to skip checking the installation media?</source>
        <translation>Você tem certeza que quer interromper a verificação da mídia de instalação?</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="190"/>
        <source>Deleting old system</source>
        <translation>Apagando o sistema antigo</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="194"/>
        <source>Failed to delete old system on destination.</source>
        <translation>Ocorreu uma falha ao excluir o sistema antigo no destino.</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="200"/>
        <source>Creating system directories</source>
        <translation>Criando os diretórios (pastas) do sistema</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="210"/>
        <source>Fixing configuration</source>
        <translation>Reparando a configuração</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="222"/>
        <source>Failed to finalize encryption setup.</source>
        <translation>Ocorreu uma falha ao finalizar a configuração da criptografia/encriptação.</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="291"/>
        <source>Copying new system</source>
        <translation>Copiando o novo sistema</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="322"/>
        <source>Failed to copy the new system.</source>
        <translation>Ocorreu uma falha ao copiar o novo sistema.</translation>
    </message>
</context>
<context>
    <name>BootMan</name>
    <message>
        <location filename="../bootman.cpp" line="248"/>
        <source>Updating initramfs</source>
        <translation>Atualizando o initramfs</translation>
    </message>
    <message>
        <location filename="../bootman.cpp" line="127"/>
        <source>Installing GRUB</source>
        <translation>Instalando o GRUB</translation>
    </message>
    <message>
        <location filename="../bootman.cpp" line="162"/>
        <source>GRUB installation failed. You can reboot to the live medium and use the GRUB Rescue menu to repair the installation.</source>
        <translation>A instalação do GRUB falhou. Você pode reparar a instalação do GRUB reiniciando o computador com o USB/DVD/CD executável e utilizar o programa &apos;Reparador de Inicialização&apos; (Boot Repair).</translation>
    </message>
    <message>
        <location filename="../bootman.cpp" line="277"/>
        <source>System boot disk:</source>
        <translation>Disco de inicialização do sistema operacional:</translation>
    </message>
    <message>
        <location filename="../bootman.cpp" line="296"/>
        <location filename="../bootman.cpp" line="308"/>
        <source>Partition to use:</source>
        <translation>Partição para ser utilizada:</translation>
    </message>
</context>
<context>
    <name>DeviceItem</name>
    <message>
        <location filename="../partman.cpp" line="1793"/>
        <source>EFI System Partition</source>
        <translation>Partição do Sistema EFI</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1794"/>
        <source>swap space</source>
        <translation>espaço de troca (swap)</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1795"/>
        <source>format only</source>
        <translation>apenas no formato</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1918"/>
        <source>Create</source>
        <translation>Criar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1921"/>
        <source>Preserve</source>
        <translation>Preservar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1922"/>
        <source>Preserve (%1)</source>
        <translation>Preservar (%1)</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1923"/>
        <source>Preserve /home (%1)</source>
        <translation>Preservar a pasta /home do (%1)</translation>
    </message>
</context>
<context>
    <name>DeviceItemDelegate</name>
    <message>
        <location filename="../partman.cpp" line="2347"/>
        <source>&amp;Templates</source>
        <translation>&amp;Modelos</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="2354"/>
        <source>Compression (&amp;ZLIB)</source>
        <translation>Compressão (&amp;ZLIB)</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="2356"/>
        <source>Compression (Z&amp;STD)</source>
        <translation>Compressão (Z&amp;STD)</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="2358"/>
        <source>Compression (&amp;LZO)</source>
        <translation>Compressão (&amp;LZO)</translation>
    </message>
</context>
<context>
    <name>MInstall</name>
    <message>
        <location filename="../minstall.cpp" line="69"/>
        <source>Shutdown</source>
        <translation>Desligar</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="138"/>
        <source>You are running 32bit OS started in 64 bit UEFI mode, the system will not be able to boot unless you select Legacy Boot or similar at restart.
We recommend you quit now and restart in Legacy Boot

Do you want to continue the installation?</source>
        <translation>Você está executando um Sistema Operacional de 32 bits que foi iniciado no modo UEFI de 64 bits. Após a instalação no disco, o sistema não inicializará, a menos que na reinicialização do computador você selecione o modo de inicialização legado/herdado (Legacy Boot), isto é, o modo BIOS, ou similar.
Recomenda-se que o sistema seja encerrado agora e reiniciado com o modo de inicialização legado.

Você quer continuar a instalação?</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="157"/>
        <source>Cannot access installation media.</source>
        <translation>Não é possível acessar a mídia de instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="173"/>
        <source>Support %1

%1 is supported by people like you. Some help others at the support forum - %2, or translate help files into different languages, or make suggestions, write documentation, or help test new software.</source>
        <translation>Apoie o %1

O %1 é apoiado por pessoas como você. Alguns ajudam outras pessoas no fórum de suporte (%2), outras pessoas traduzem arquivos de ajuda para diferentes idiomas, ou fazem sugestões, elaboram documentação ou ajudam a testar os novos programas (software).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="223"/>
        <source>%1 is an independent Linux distribution based on Debian Stable.

%1 uses some components from MEPIS Linux which are released under an Apache free license. Some MEPIS components have been modified for %1.

Enjoy using %1</source>
        <translation>O %1 é uma distribuição GUN/Linux independente baseada na distribuição Debian Estável (Stable).

O %1 utiliza alguns componentes do MEPIS Linux que foram publicados sob a licença livre do Apache (Apache free license). Alguns componentes do MEPIS foram modificados para serem utilizados no %1.

Divirta-se utilizando o %1.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="359"/>
        <source>Pretending to install %1</source>
        <translation>Simulando a instalação do %1</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="379"/>
        <source>Preparing to install %1</source>
        <translation>Preparando para instalar o %1</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="401"/>
        <source>Paused for required operator input</source>
        <translation>Aguardando o operador inserir as informações requeridas</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="412"/>
        <source>Setting system configuration</source>
        <translation>Definir a configuração do sistema</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="426"/>
        <source>Cleaning up</source>
        <translation>Limpando</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="428"/>
        <source>Finished</source>
        <translation>Finalizado</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="526"/>
        <source>Invalid settings found in configuration file (%1). Please review marked fields as you encounter them.</source>
        <translation>Foram encontradas definições inválidas no arquivo de configurações (%1). Por favor, reveja os campos marcados à medida que você os encontrar.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="568"/>
        <source>OK to format and use the entire disk (%1) for %2?</source>
        <translation>Formatar e utilizar todo o disco (%1) para o %2?</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="572"/>
        <source>WARNING: The selected drive has a capacity of at least 2TB and must be formatted using GPT. On some systems, a GPT-formatted disk will not boot.</source>
        <translation>AVISO: O disco selecionado possui capacidade de pelo menos 2 TB e tem que ser formatado utilizando o GPT (GUID Partition Table). Em alguns sistemas, um disco formatado em GPT não inicializará.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="600"/>
        <source>The data in /home cannot be preserved because the required information could not be obtained.</source>
        <translation>Os dados em /home não podem ser preservados porque as informações requeridas para este processo não puderam ser obtidas.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="615"/>
        <source>The home directory for %1 already exists.</source>
        <translation>A pasta home para %1 já existe.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="662"/>
        <source>General Instructions</source>
        <translation>Instruções Gerais</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="663"/>
        <source>BEFORE PROCEEDING, CLOSE ALL OTHER APPLICATIONS.</source>
        <translation>ANTES DE PROSSEGUIR, FECHE TODOS OS OUTROS APLICATIVOS.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="664"/>
        <source>On each page, please read the instructions, make your selections, and then click on Next when you are ready to proceed. You will be prompted for confirmation before any destructive actions are performed.</source>
        <translation>Em cada página, leia as instruções atentamente, faça as suas escolhas e clique no botão «Próximo». Será solicitado a sua confirmação, antes de continuar quaisquer ações destrutivas (ações que apagam ou excluem os seus dados do seu dispositivo de armazenamento) a serem executadas.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="666"/>
        <source>Limitations</source>
        <translation>Limitações</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="667"/>
        <source>Remember, this software is provided AS-IS with no warranty what-so-ever. It is solely your responsibility to backup your data before proceeding.</source>
        <translation>Lembre-se de que este sistema operacional (software) é disponibilizado COMO ESTÁ, sem qualquer tipo de garantia. É de sua exclusiva responsabilidade fazer a cópia de segurança (backup) dos seus dados que estejam no dispositivo de armazenamento do computador, antes de continuar com o processo de instalação do antiX.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="680"/>
        <source>Installation Options</source>
        <translation>Opções de Instalação</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="681"/>
        <source>Installation requires about %1 of space. %2 or more is preferred.</source>
        <translation>A instalação requer cerca de %1 de espaço. É preferível %2 ou mais.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="682"/>
        <source>If you are running Mac OS or Windows OS (from Vista onwards), you may have to use that system&apos;s software to set up partitions and boot manager before installing.</source>
        <translation>Se você estiver executando um sistema operacional Mac OS ou Windows (Vista ou posterior), pode ser necessário utilizar o programa (software) do sistema existente para configurar as partições e instalar o gerenciador de inicialização (boot manager) antes de prosseguir com esta instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="683"/>
        <source>Using the root-home space slider</source>
        <translation>Utilizando o controle deslizante de espaço root-home.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="684"/>
        <source>The drive can be divided into separate system (root) and user data (home) partitions using the slider.</source>
        <translation>O dispositivo de armazenamento pode ser dividido em partições separadas, sendo uma partição para a instalação do sistema operacional (root) e outra partição para o armazenamento de dados da pasta pessoal do usuário (home). Para isto, utilize o controle deslizante.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="685"/>
        <source>The &lt;b&gt;root&lt;/b&gt; partition will contain the operating system and applications.</source>
        <translation>A partição &lt;b&gt;root&lt;/b&gt; (raiz) conterá o sistema operacional e os aplicativos (tanto os aplicativos pré-instalados no antiX, quanto os aplicativos que vierem a ser instalados pelo usuário).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="686"/>
        <source>The &lt;b&gt;home&lt;/b&gt; partition will contain the data of all users, such as their settings, files, documents, pictures, music, videos, etc.</source>
        <translation>A partição &lt;b&gt;home&lt;/b&gt; conterá os dados de todos os usuários (pasta pessoal ou pasta do usuário), como suas configurações, arquivos, documentos, imagens, músicas, vídeos, etc.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="687"/>
        <source>Move the slider to the right to increase the space for &lt;b&gt;root&lt;/b&gt;. Move it to the left to increase the space for &lt;b&gt;home&lt;/b&gt;.</source>
        <translation>Mova o controle deslizante para a direita para aumentar o espaço para &lt;b&gt;root&lt;/b&gt;. Mova o controle deslizante para a esquerda para aumentar o espaço para &lt;b&gt;home&lt;/b&gt;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="688"/>
        <source>Move the slider all the way to the right if you want both root and home on the same partition.</source>
        <translation>Mova o controle deslizante totalmente para a direita se você quiser root e home na mesma partição.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="689"/>
        <source>Keeping the home directory in a separate partition improves the reliability of operating system upgrades. It also makes backing up and recovery easier. This can also improve overall performance by constraining the system files to a defined portion of the drive.</source>
        <translation>Manter a pasta home em uma partição separada aumenta a confiabilidade nas atualizações do sistema operacional. Também torna a cópia de segurança e a recuperação dos dados mais fáceis. Isto também pode melhorar o desempenho geral, restringindo os arquivos do sistema a uma parte definida do dispositivo de armazenamento.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="691"/>
        <location filename="../minstall.cpp" line="774"/>
        <source>Encryption</source>
        <translation>Criptografia ou Encriptação</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="692"/>
        <location filename="../minstall.cpp" line="775"/>
        <source>Encryption is possible via LUKS. A password is required.</source>
        <translation>A criptografia/encriptação é possível via LUKS. É necessária uma senha.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="693"/>
        <location filename="../minstall.cpp" line="776"/>
        <source>A separate unencrypted boot partition is required.</source>
        <translation>É necessária uma partição de inicialização separada e não criptografada.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="694"/>
        <source>When encryption is used with autoinstall, the separate boot partition will be automatically created.</source>
        <translation>Quando a criptografia/encriptação é utilizada com uma instalação automática, será automaticamente criada uma partição de inicialização (boot partition) separada.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="695"/>
        <source>Using a custom disk layout</source>
        <translation>Utilizando um leiaute de disco personalizado</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="696"/>
        <source>If you need more control over where %1 is installed to, select &quot;&lt;b&gt;%2&lt;/b&gt;&quot; and click &lt;b&gt;Next&lt;/b&gt;. On the next page, you will then be able to select and configure the storage devices and partitions you need.</source>
        <translation>Se você precisar de mais controle sobre onde %1 será instalado, selecione &quot;&lt;b&gt;%2&lt;/b&gt;&quot; e clique em &lt;b&gt;Próximo&lt;/b&gt;. Na próxima página, você poderá selecionar e configurar os dispositivos de armazenamento e as partições que você precisa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="710"/>
        <source>Choose Partitions</source>
        <translation>Escolher as Partições</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="711"/>
        <source>The partition list allows you to choose what partitions are used for this installation.</source>
        <translation>A lista de partições permite que você escolha quais partições são utilizadas para esta instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="712"/>
        <source>&lt;i&gt;Device&lt;/i&gt; - This is the block device name that is, or will be, assigned to the created partition.</source>
        <translation>&lt;i&gt;Dispositivo&lt;/i&gt; - Este é o nome do dispositivo de bloco (dispositivo de armazenamento) que é, ou que será, atribuído à partição criada.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="713"/>
        <source>&lt;i&gt;Size&lt;/i&gt; - The size of the partition. This can only be changed on a new layout.</source>
        <translation>&lt;i&gt;Tamanho&lt;/i&gt; - O tamanho da partição só pode ser alterado em um novo leiaute.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="714"/>
        <source>&lt;i&gt;Use For&lt;/i&gt; - To use this partition in an installation, you must select something here.</source>
        <translation>&lt;i&gt;Utilizar Para&lt;/i&gt; - Você tem que selecionar esta partição para utilizar em uma instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="715"/>
        <source>Format - Format without mounting.</source>
        <translation>Formato - Formatar sem montar a partição.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="716"/>
        <source>BIOS-GRUB - BIOS Boot GPT partition for GRUB.</source>
        <translation>BIOS-GRUB - partição GPT de inicialização do BIOS para o GRUB.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="717"/>
        <source>EFI - EFI System Partition.</source>
        <translation>EFI - Sistema de Partição EFI.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="718"/>
        <source>boot - Boot manager (/boot).</source>
        <translation>boot - Gerenciador de Inicialização (/boot).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="719"/>
        <source>root - System root (/).</source>
        <translation>root - Raiz do Sistema (/).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="720"/>
        <source>swap - Swap space.</source>
        <translation>swap - Partição de Troca.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="721"/>
        <source>home - User data (/home).</source>
        <translation>home - Dados do Usuário (/home).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="722"/>
        <source>In addition to the above, you can also type your own mount point. Custom mount points must start with a slash (&quot;/&quot;).</source>
        <translation>Além do que foi referido acima, você também pode digitar o seu próprio ponto de montagem. Os pontos de montagem personalizados devem começar com uma barra (&quot;/&quot;).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="723"/>
        <source>The installer treats &quot;/boot&quot;, &quot;/&quot;, and &quot;/home&quot; exactly the same as &quot;boot&quot;, &quot;root&quot;, and &quot;home&quot;, respectively.</source>
        <translation>O instalador trata respectivamente, &quot;/boot&quot;, &quot;/&quot; e &quot;/home&quot; exatamente da mesma forma que &quot;boot&quot;, &quot;root&quot; e &quot;home&quot;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="724"/>
        <source>&lt;i&gt;Label&lt;/i&gt; - The label that is assigned to the partition once it has been formatted.</source>
        <translation>Rótulo - O rótulo que é atribuído à partição depois de ter sido formatado.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="725"/>
        <source>&lt;i&gt;Encrypt&lt;/i&gt; - Use LUKS encryption for this partition. The password applies to all partitions selected for encryption.</source>
        <translation>&lt;i&gt;Criptografar&lt;/i&gt; ou &lt;i&gt;Encriptar&lt;/i&gt; - Utilizar a criptografia/encriptação LUKS para esta partição. A senha se aplica a todas as partições selecionadas para criptografia.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="726"/>
        <source>&lt;i&gt;Format&lt;/i&gt; - This is the partition&apos;s format. Available formats depend on what the partition is used for. When working with an existing layout, you may be able to preserve the format of the partition by selecting &lt;b&gt;Preserve&lt;/b&gt;.</source>
        <translation>&lt;i&gt;Formato&lt;/i&gt; - Este é o formato da partição. Os formatos disponíveis dependem para o que a partição será utilizada. Ao trabalhar com um leiaute existente, você pode ser capaz de preservar o formato da partição, selecionando &lt;b&gt;Preservar&lt;/b&gt;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="728"/>
        <source>Selecting &lt;b&gt;Preserve /home&lt;/b&gt; for the root partition preserves the contents of the /home directory, deleting everything else. This option can only be used when /home is on the same partition as the root.</source>
        <translation>Selecionar &lt;b&gt;Preservar a pasta pessoal (/home)&lt;/b&gt; para a partição raiz (root) preservar o conteúdo da pasta pessoal (/home), excluindo todo o restante. Esta opção só pode ser utilizada quando pasta pessoal (/home) estiver na mesma partição da partição raiz (root).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="730"/>
        <source>The ext2, ext3, ext4, jfs, xfs and btrfs Linux filesystems are supported and ext4 is recommended.</source>
        <translation>Os sistemas de arquivos do GNU/Linux do tipo ext2, ext3, ext4, jfs, xfs e btrfs são suportados e o ext4 é o formato recomendado.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="731"/>
        <source>&lt;i&gt;Check&lt;/i&gt; - Check and correct for bad blocks on the drive (not supported for all formats). This is very time consuming, so you may want to skip this step unless you suspect that your drive has bad blocks.</source>
        <translation>&lt;i&gt;Verificar&lt;/i&gt; - Verifica e corrige os blocos defeituosos no dispositivo (não é compatível com todos os formatos). Isto consome muito tempo, então você pode querer pular esta etapa, a menos que suspeite que o seu dispositivo possui blocos defeituosos.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="733"/>
        <source>&lt;i&gt;Mount Options&lt;/i&gt; - This specifies mounting options that will be used for this partition.</source>
        <translation>&lt;i&gt;Opções de Montagem&lt;/i&gt; - Especifica as opções de montagem que serão utilizadas para esta partição.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="734"/>
        <source>&lt;i&gt;Dump&lt;/i&gt; - Instructs the dump utility to include this partition in the backup.</source>
        <translation>&lt;i&gt;Dump (Despejar)&lt;/i&gt; - Transmite ao utilitário &apos;dump&apos; instruções para incluir esta partição na cópia de segurança.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="735"/>
        <source>&lt;i&gt;Pass&lt;/i&gt; - The sequence in which this file system is to be checked at boot. If zero, the file system is not checked.</source>
        <translation>&lt;i&gt;Pass (Passar)&lt;/i&gt; - A seqüência na qual este sistema de arquivos deve ser verificado na inicialização. Se for zero, o sistema de arquivos não será verificado.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="736"/>
        <source>Menus and actions</source>
        <translation>Menus e ações</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="737"/>
        <source>A variety of actions are available by right-clicking any drive or partition item in the list.</source>
        <translation>Uma variedade de ações estão disponíveis clicando com o botão direito do rato/mouse em qualquer unidade ou item de partição da lista.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="738"/>
        <source>The buttons to the right of the list can also be used to manipulate the entries.</source>
        <translation>Os botões à direita da lista também podem ser utilizados para manipular as entradas.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="739"/>
        <source>The installer cannot modify the layout already on the drive. To create a custom layout, mark the drive for a new layout with the &lt;b&gt;New layout&lt;/b&gt; menu action or button (%1). This clears the existing layout.</source>
        <translation>O instalador não pode modificar o leiaute já existente em uma unidade. Para criar um leiaute personalizado, marque a unidade para um novo leiaute com a ação de menu &lt;b&gt;Novo Leiaute&lt;/b&gt; ou com o botão (%1). Isto limpará o leiaute existente.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="742"/>
        <source>Basic layout requirements</source>
        <translation>Requisitos básicos do leiaute</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="743"/>
        <source>%1 requires a root partition. The swap partition is optional but highly recommended. If you want to use the Suspend-to-Disk feature of %1, you will need a swap partition that is larger than your physical memory size.</source>
        <translation>O %1 requer uma partição raiz ( /, root). A partição de trocas (swap) é opcional, mas é altamente recomendada. Se você quiser utilizar o recurso Hibernar (Suspender para o Disco) do %1, você precisará de uma partição de trocas (swap) maior do que a memória RAM física disponível no seu computador.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="745"/>
        <source>If you choose a separate /home partition it will be easier for you to upgrade in the future, but this will not be possible if you are upgrading from an installation that does not have a separate home partition.</source>
        <translation>Se você escolher uma partição /home separada facilitará no futuro a substituição do sistema atual por uma nova versão. Mas se o sistema instalado não tiver em uma partição /home separada, não será possível criá-la posteriormente.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="747"/>
        <source>Active partition</source>
        <translation>Partição Ativa</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="748"/>
        <source>For the installed operating system to boot, the appropriate partition (usually the boot or root partition) must be the marked as active.</source>
        <translation>Para que o sistema operacional instalado seja inicializado, a partição apropriada (geralmente a partição de inicialização de root ou raiz) deve ser marcada como ativa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="749"/>
        <source>The active partition of a drive can be chosen using the &lt;b&gt;Active partition&lt;/b&gt; menu action.</source>
        <translation>A partição ativa de uma unidade pode ser escolhida utilizando a ação do menu &lt;b&gt;Partição Ativa&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="750"/>
        <source>A partition with an asterisk (*) next to its device name is, or will become, the active partition.</source>
        <translation>Uma partição com um asterisco (*) ao lado do nome do dispositivo é, ou se tornará, a partição ativa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="751"/>
        <source>Boot partition</source>
        <translation>Partição de inicialização</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="752"/>
        <source>This partition is generally only required for root partitions on virtual devices such as encrypted, LVM or software RAID volumes.</source>
        <translation>Esta partição é geralmente necessária apenas para partições de root (raiz) em dispositivos virtuais como volumes criptografados/encriptados por programas LVM (Logical Volume Manager) ou RAID (Redundant Array of Inexpensive Drives).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="753"/>
        <source>It contains a basic kernel and drivers used to access the encrypted disk or virtual devices.</source>
        <translation>Ele contém um núcleo (kernel) básico e controladores (drivers) utilizados para acessar o disco criptografado/encriptado ou dispositivos virtuais.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="754"/>
        <source>BIOS-GRUB partition</source>
        <translation>Partição BIOS-GRUB</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="755"/>
        <source>When using a GPT-formatted drive on a non-EFI system, a 1MB BIOS boot partition is required when using GRUB.</source>
        <translation>Ao utilizar uma unidade com formato GPT em um sistema não EFI, uma partição de inicialização do BIOS de 1MB é necessária ao utilizar o GRUB.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="756"/>
        <source>New drives are formatted in GPT if more than 4 partitions are to be created, or the drive has a capacity greater than 2TB. If the installer is about to format the disk in GPT, and there is no BIOS-GRUB partition, a warning will be displayed before the installation starts.</source>
        <translation>As novas unidades são formatadas em GPT se mais de 4 partições forem criadas ou se a unidade tiver uma capacidade maior que 2 TB. Se o instalador estiver prestes a formatar o disco em GPT e não houver a partição BIOS-GRUB, um aviso será exibido antes do início da instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="758"/>
        <source>Need help creating a layout?</source>
        <translation>Você precisa de ajuda para criar um leiaute?</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="759"/>
        <source>Just right-click on a drive to bring up a menu, and select a layout template. These layouts are similar to that of the regular install.</source>
        <translation>Basta clicar com o botão direito do rato/mouse em uma unidade para abrir um menu e selecionar um modelo de leiaute. Estes leiautes são semelhantes aos da instalação normal.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="760"/>
        <source>&lt;i&gt;Standard install&lt;/i&gt; - Suited to most setups. This template does not add a separate boot partition, and so it is unsuitable for use with an encrypted operating system.</source>
        <translation>&lt;i&gt;Instalação Padrão&lt;/i&gt; - É adequada para a maioria das configurações. Este modelo não adiciona uma partição de inicialização separada e, portanto, não é adequado para utilização com um sistema operacional criptografado.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="761"/>
        <source>&lt;i&gt;Encrypted system&lt;/i&gt; - Contains the boot partition required to load an encrypted operating system. This template can also be used as the basis for a multi-boot system.</source>
        <translation>&lt;i&gt;Sistema Criptografado  ou Encriptado&lt;/i&gt; - Contém a partição de inicialização necessária para carregar um sistema operacional criptografado/encriptado. Este modelo também pode ser utilizado como base para um sistema de inicialização múltipla.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="762"/>
        <source>Upgrading</source>
        <translation>Atualizando</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="763"/>
        <source>To upgrade from an existing Linux installation, select the same home partition as before and select &lt;b&gt;Preserve&lt;/b&gt; as the format.</source>
        <translation>Para atualizar a partir de uma instalação GNU/Linux existente, selecione a mesma partição home de antes e selecione &lt;b&gt;Preservar&lt;/b&gt; como o formato.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="764"/>
        <source>If you do not use a separate home partition, select &lt;b&gt;Preserve /home&lt;/b&gt; on the root file system entry to preserve the existing /home directory located on your root partition. The installer will only preserve /home, and will delete everything else. As a result, the installation will take much longer than usual.</source>
        <translation>Se você não utilizar uma partição home separada, selecione &lt;b&gt;Preservar /home&lt;/b&gt; na entrada do sistema de arquivos root (raiz) para preservar o diretório /home existente localizado em sua partição root. O instalador preservará apenas /home e excluirá ou apagará todo o resto. Como resultado, a instalação irá demorar muito mais do que o habitual.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="766"/>
        <source>Preferred Filesystem Type</source>
        <translation>Tipo de Sistema de Arquivos Preferido</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="767"/>
        <source>For %1, you may choose to format the partitions as ext2, ext3, ext4, f2fs, jfs, xfs or btrfs.</source>
        <translation>Para %1, você pode optar por formatar as partições como ext2, ext3, ext4, f2fs, jfs, xfs ou btrfs.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="768"/>
        <source>Additional compression options are available for drives using btrfs. Lzo is fast, but the compression is lower. Zlib is slower, with higher compression.</source>
        <translation>A partições formatadas como btrfs podem utilizar opções adicionais de compressão. O sistema lzo proporciona rapidez de compressão, mas a taxa de compressão é menor. O sistema zlib proporciona maior compressão, mas menor rapidez.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="770"/>
        <source>System partition management tool</source>
        <translation>Ferramenta de gerenciamento de partição do sistema</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="771"/>
        <source>For more control over the drive layouts (such as modifying the existing layout on a disk), click the partition management button (%1). This will run the operating system&apos;s partition management tool, which will allow you to create the exact layout you need.</source>
        <translation>Para obter mais controle sobre os leiautes da unidade (como modificar o leiaute existente em um disco), clique no botão de gerenciamento de partição (%1). Esta opção executará a ferramenta de gerenciamento de partição do sistema operacional, que permitirá que você crie o leiaute exato de que precisa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="777"/>
        <source>To preserve an encrypted partition, right-click on it and select &lt;b&gt;Unlock&lt;/b&gt;. In the dialog that appears, enter a name for the virtual device and the password. When the device is unlocked, the name you chose will appear under &lt;i&gt;Virtual Devices&lt;/i&gt;, with similar options to that of a regular partition.</source>
        <translation>Para preservar uma partição criptografada, clique com o botão direito do rato/mouse sobre ela e selecione &lt;b&gt;Desbloquear&lt;/b&gt;. Na caixa de diálogo que aparece, insira um nome e a senha para o dispositivo virtual. Quando o dispositivo está desbloqueado, o nome que você escolheu aparecerá em &lt;i&gt;Dispositivos Virtuais&lt;/i&gt;, com opções similares às de uma partição normal.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="779"/>
        <source>For the encrypted partition to be unlocked at boot, it needs to be added to the crypttab file. Use the &lt;b&gt;Add to crypttab&lt;/b&gt; menu action to do this.</source>
        <translation>Para que a partição criptografada/encriptada seja desbloqueada na inicialização, ela precisa ser adicionada ao arquivo crypttab. Utilize a ação do menu &lt;b&gt;Adicionar ao crypttab&lt;/b&gt; para fazer isto.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="780"/>
        <source>Other partitions</source>
        <translation>Outras partições</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="781"/>
        <source>The installer allows other partitions to be created or used for other purposes, however be mindful that older systems cannot handle drives with more than 4 partitions.</source>
        <translation>O instalador permite que outras partições sejam criadas ou utilizadas para outros fins, no entanto, esteja ciente de que os sistemas mais antigos não podem lidar com acionamentos de unidades com mais de 4 partições.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="782"/>
        <source>Subvolumes</source>
        <translation>Subvolumes</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="783"/>
        <source>Some file systems, such as Btrfs, support multiple subvolumes in a single partition. These are not physical subdivisions, and so their order does not matter.</source>
        <translation>Alguns sistemas de arquivos, como Btrfs, oferecem suporte a vários subvolumes em uma única partição. Estas não são subdivisões físicas e, portanto, sua ordem não importa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="785"/>
        <source>Use the &lt;b&gt;Scan subvolumes&lt;/b&gt; menu action to search an existing Btrfs partition for subvolumes. To create a new subvolume, use the &lt;b&gt;New subvolume&lt;/b&gt; menu action.</source>
        <translation>Utilize a ação do menu &lt;b&gt;Verificar os subvolumes&lt;/b&gt; para pesquisar os subvolumes em uma partição Btrfs existente. Para criar um novo subvolume, utilize a ação do menu &lt;b&gt;Novo subvolume&lt;/b&gt;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="787"/>
        <source>Existing subvolumes can be preserved, however the name must remain the same.</source>
        <translation>Os subvolumes existentes podem ser preservados, mas o nome tem que permanecer o mesmo.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="788"/>
        <source>Virtual Devices</source>
        <translation>Dispositivos Virtuais</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="789"/>
        <source>If the intaller detects any virtual devices such as opened LUKS partitions, LVM logical volumes or software-based RAID volumes, they may be used for the installation.</source>
        <translation>Se o instalador detectar quaisquer dispositivos virtuais, como partições LUKS abertas, volumes lógicos LVM ou volumes RAID baseados em programa (software), eles podem ser utilizados para a instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="790"/>
        <source>The use of virtual devices (beyond preserving encrypted file systems) is an advanced feature. You may have to edit some files (eg. initramfs, crypttab, fstab) to ensure the virtual devices used are created upon boot.</source>
        <translation>A utilização de dispositivos virtuais (além de preservar sistemas de arquivos criptografados) é um recurso avançado. Você pode ter que editar alguns arquivos (por exemplo: initramfs, crypttab e fstab) para garantir que os dispositivos virtuais utilizados sejam criados na inicialização.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="795"/>
        <source>Install GRUB for Linux and Windows</source>
        <translation type="unfinished">Instalar o GRUB para o GNU/Linux e o Windows</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="796"/>
        <source>%1 uses the GRUB bootloader to boot %1 and Microsoft Windows.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="797"/>
        <source>By default GRUB is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="798"/>
        <source>If you choose to install GRUB to Partition Boot Record (PBR) instead, then GRUB will be installed at the beginning of the specified partition. This option is for experts only.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="799"/>
        <source>If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="800"/>
        <source>Create a swap file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="801"/>
        <source>A swap file is more flexible than a swap partition; it is considerably easier to resize a swap file to adapt to changes in system usage.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="802"/>
        <source>By default, this is checked if no swap partitions have been set, and unchecked if swap partitions are set. This option should be left untouched, and is for experts only.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="803"/>
        <source>Setting the size to 0 has the same effect as unchecking this option.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>&lt;p&gt;&lt;b&gt;Select Boot Method&lt;/b&gt;&lt;br/&gt; %1 uses the GRUB bootloader to boot %1 and MS-Windows. &lt;p&gt;By default GRUB2 is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.&lt;/p&gt;&lt;p&gt;If you choose to install GRUB2 to Partition Boot Record (PBR) instead, then GRUB2 will be installed at the beginning of the specified partition. This option is for experts only.&lt;/p&gt;&lt;p&gt;If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.&lt;/p&gt;</source>
        <translation type="vanished">&lt;p&gt;&lt;b&gt;Selecione o Método de Inicialização&lt;/b&gt;&lt;br/&gt;O %1 utiliza o gerenciador/carregador de inicialização (bootloader) GRUB2 para inicializar o sistema operacional %1, Windows ou outro instalado. &lt;p&gt;Por padrão, o GRUB2 será instalado no MBR [Master Boot Record] ou na ESP [EFI System Partition] (no caso de sistemas de 64 bits com UEFI) do disco (do disco de inicialização se o computador tiver mais do que um disco) e substituirá o gerenciador/carregador de inicialização que estava instalado antes.&lt;/p&gt;&lt;p&gt;Se você optar por instalar o GRUB2 na raíz do sistema, ele será instalado no início da partição escolhida. Usuários inexperientes não devem utilizar esta opção.&lt;/p&gt;&lt;p&gt;Se você desmarcar a opção &apos;Instalar o GRUB para o GNU/Linux e Windows&apos;, o GRUB não será instalado neste momento. Os usuários inexperientes não devem desmarcar esta opção.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="814"/>
        <source>&lt;p&gt;&lt;b&gt;Common Services to Enable&lt;/b&gt;&lt;br/&gt;Select any of these common services that you might need with your system configuration and the services will be started automatically when you start %1.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Ativar os Serviços de Uso Frequente&lt;/b&gt;&lt;br/&gt;Selecione quaisquer destes serviços comuns e eles serão iniciados automaticamente ao iniciar o %1.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="818"/>
        <source>&lt;p&gt;&lt;b&gt;Computer Identity&lt;/b&gt;&lt;br/&gt;The computer name is a common unique name which will identify your computer if it is on a network. The computer domain is unlikely to be used unless your ISP or local network requires it.&lt;/p&gt;&lt;p&gt;The computer and domain names can contain only alphanumeric characters, dots, hyphens. They cannot contain blank spaces, start or end with hyphens&lt;/p&gt;&lt;p&gt;The SaMBa Server needs to be activated if you want to use it to share some of your directories or printer with a local computer that is running MS-Windows or Mac OSX.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Identificação do Computador&lt;/b&gt;&lt;br/&gt;O nome do computador é um nome específico que permite identificar o computador em uma rede a que esteja conectado. Não é provável que o domínio do computador seja utilizado, a menos que o seu provedor de serviços de internet (ISP) ou que a sua rede local exija esta informação.&lt;/p&gt;&lt;p&gt;O nome de computador e de domínio podem conter apenas caracteres alfanuméricos, pontos e hifens. Eles não podem conter espaços em branco, nem começar ou terminar com hifens.&lt;/p&gt;&lt;p&gt;Para compartilhar pastas/diretórios ou uma impressora com um computador local que opere com o Windows ou Mac OSX é necessário ativar o servidor de arquivos (aplicativo) Samba.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="828"/>
        <source>Localization Defaults</source>
        <translation>Padrões de Localização</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="829"/>
        <source>Set the default locale. This will apply unless they are overridden later by the user.</source>
        <translation>Defina o local padrão. Isto se aplicará a menos que sejam substituídos posteriormente pelo usuário.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="830"/>
        <source>Configure Clock</source>
        <translation>Configurar o Relógio</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="831"/>
        <source>If you have an Apple or a pure Unix computer, by default the system clock is set to Greenwich Meridian Time (GMT) or Coordinated Universal Time (UTC). To change this, check the &quot;&lt;b&gt;System clock uses local time&lt;/b&gt;&quot; box.</source>
        <translation>Se você tiver um computador Apple ou um Unix puro, por padrão, o relógio do sistema é definido para o Horário do Meridiano de Greenwich (Greenwich Meridian Time - GMT) ou Tempo Universal Coordenado (Coordinated Universal Time - UTC). Para alterar esta opção, marque a caixa &quot;&lt;b&gt;O relógio do sistema utiliza a hora local&lt;/b&gt;&quot;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="833"/>
        <source>The system boots with the timezone preset to GMT/UTC. To change the timezone, after you reboot into the new installation, right click on the clock in the Panel and select Properties.</source>
        <translation>O sistema inicializa com o fuso horário predefinido para GMT/UTC. Para alterar o fuso horário, após reiniciar na nova instalação, clique com o botão direito do rato/mouse sobre o relógio no Painel e selecione Propriedades.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="835"/>
        <source>Service Settings</source>
        <translation>Configurações de Serviços</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="836"/>
        <source>Most users should not change the defaults. Users with low-resource computers sometimes want to disable unneeded services in order to keep the RAM usage as low as possible. Make sure you know what you are doing!</source>
        <translation>A maioria dos usuários não devem alterar os padrões. Os usuários com computadores que possuem poucos recursos (processamento e memória RAM), podem querer desativar alguns serviços desnecessários para manter o uso da memória RAM o mais baixo possível. Certifique-se de que você sabe o que está fazendo!</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="842"/>
        <source>Default User Login</source>
        <translation>Acesso (Login) do Usuário Padrão</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="843"/>
        <source>The root user is similar to the Administrator user in some other operating systems. You should not use the root user as your daily user account. Please enter the name for a new (default) user account that you will use on a daily basis. If needed, you can add other user accounts later with %1 User Manager.</source>
        <translation>O usuário root é semelhante ao usuário Administrador em outros sistemas operacionais. Você não deve utilizar o usuário root como a sua conta de usuário diária. Por favor, insira o nome de uma nova conta de usuário (padrão) que você utilizará diariamente. Se necessário, você poderá adicionar outras contas de usuário posteriormente com o Gerenciador de Usuários do %1 (%1 User Manager).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="847"/>
        <source>Passwords</source>
        <translation>Senhas </translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="848"/>
        <source>Enter a new password for your default user account and for the root account. Each password must be entered twice.</source>
        <translation>Insira uma nova senha para sua conta de usuário padrão e para a conta de root. Cada senha deve ser inserida duas vezes.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="850"/>
        <source>No passwords</source>
        <translation>Nenhuma senha</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="851"/>
        <source>If you want the default user account to have no password, leave its password fields empty. This allows you to log in without requiring a password.</source>
        <translation>Se você quiser que a conta de usuário padrão não tenha senha, deixe os campos de senha em branco. Isso permite que você faça o acesso (login) sem exigir uma senha.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="853"/>
        <source>Obviously, this should only be done in situations where the user account does not need to be secure, such as a public terminal.</source>
        <translation>Obviamente, isto só deve ser feito em situações onde a conta do usuário não precisa ser segura, como por exemplo, em um terminal público.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="861"/>
        <source>Old Home Directory</source>
        <translation>Pasta Home Antiga</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="862"/>
        <source>A home directory already exists for the user name you have chosen. This screen allows you to choose what happens to this directory.</source>
        <translation>Já existe uma pasta pessoal (diretório pessoal) para o nome de usuário que você escolheu. Esta tela permite que você escolha o que fazer com esta pasta ou diretório.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="864"/>
        <source>Re-use it for this installation</source>
        <translation>Reutilize-a para esta instalação</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="865"/>
        <source>The old home directory will be used for this user account. This is a good choice when upgrading, and your files and settings will be readily available.</source>
        <translation>A pasta (diretório) &apos;home&apos; antiga será utilizada para esta conta de usuário. Esta é uma boa opção no caso de atualização do sistema operacional, pois os arquivos e configurações do usuário estarão imediatamente disponíveis.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="867"/>
        <source>Rename it and create a new directory</source>
        <translation>Renomear e criar uma nova pasta (diretório)</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="868"/>
        <source>A new home directory will be created for the user, but the old home directory will be renamed. Your files and settings will not be immediately visible in the new installation, but can be accessed using the renamed directory.</source>
        <translation>Será criada uma nova pasta pessoal (diretório pessoal) para o usuário e a pasta antiga será renomeada. Os arquivos e as configurações do usuário não ficarão imediatamente visíveis na nova instalação, mas podem ser acessados utilizando a pasta renomeada.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="870"/>
        <source>The old directory will have a number at the end of it, depending on how many times the directory has been renamed before.</source>
        <translation>A pasta pessoal (diretório pessoal) antiga receberá um número no final do nome, que dependerá da quantidade de vezes que a pasta tenha sido renomeada antes.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="871"/>
        <source>Delete it and create a new directory</source>
        <translation>Apagar e criar uma nova pasta (diretório)</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="872"/>
        <source>The old home directory will be deleted, and a new one will be created from scratch.</source>
        <translation>A pasta pessoal (diretório pessoal) antiga será apagada e será criada um nova.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="873"/>
        <source>Warning</source>
        <translation>Aviso</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="874"/>
        <source>All files and settings will be deleted permanently if this option is selected. Your chances of recovering them are low.</source>
        <translation>Se esta opção for selecionada, todos os arquivos e configurações serão excluídos permanentemente. A probabilidade de virem a ser recuperados é baixa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="890"/>
        <source>Installation in Progress</source>
        <translation>Instalação em Andamento</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="891"/>
        <source>%1 is installing. For a fresh install, this will probably take 3-20 minutes, depending on the speed of your system and the size of any partitions you are reformatting.</source>
        <translation>O %1 está sendo instalado. Para uma nova instalação, provavelmente irá demorar de 3 a 20 minutos, dependendo da velocidade do seu equipamento e do tamanho das partições que estão sendo reformatadas.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="893"/>
        <source>If you click the Abort button, the installation will be stopped as soon as possible.</source>
        <translation>Se você clicar no botão &apos;Abortar&apos;, a instalação será interrompida assim que for possível.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="895"/>
        <source>Change settings while you wait</source>
        <translation>Altere as configurações enquanto você espera</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="896"/>
        <source>While %1 is being installed, you can click on the &lt;b&gt;Next&lt;/b&gt; or &lt;b&gt;Back&lt;/b&gt; buttons to enter other information required for the installation.</source>
        <translation>Enquanto o %1 está sendo instalado, é possível inserir outras informações requeridas clicando nos botões &lt;b&gt;Próximo&lt;/b&gt; ou &lt;b&gt;Voltar&lt;/b&gt;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="898"/>
        <source>Complete these steps at your own pace. The installer will wait for your input if necessary.</source>
        <translation>Complete estas etapas sem pressa. O instalador aguardará estas informações se for necessário.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="905"/>
        <source>&lt;p&gt;&lt;b&gt;Congratulations!&lt;/b&gt;&lt;br/&gt;You have completed the installation of %1&lt;/p&gt;&lt;p&gt;&lt;b&gt;Finding Applications&lt;/b&gt;&lt;br/&gt;There are hundreds of excellent applications installed with %1 The best way to learn about them is to browse through the Menu and try them. Many of the apps were developed specifically for the %1 project. These are shown in the main menus. &lt;p&gt;In addition %1 includes many standard Linux applications that are run only from the command line and therefore do not show up in the Menu.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Parabéns!&lt;/b&gt;&lt;br/&gt;Você concluiu a instalação do %1 com sucesso.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Encontre os Aplicativos&lt;/b&gt;&lt;br/&gt;Existem centenas de aplicativos (aplicações) excelentes instalados no %1. A melhor maneira de se familiarizar sobre eles é navegar pelo Menu e experimentá-los. Muitos dos aplicativos foram desenvolvidas especificamente para o projeto %1. Estes são acessados pelos menus principais. &lt;p&gt;Além disso, o %1 inclui também muitos aplicativos padrão do GNU/Linux que só são executados a partir da linha de comando no terminal (console), portanto, não são exibidos no Menu.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="914"/>
        <source>Enjoy using %1&lt;/b&gt;&lt;/p&gt;</source>
        <translation>Aproveite o %1&lt;/b&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="915"/>
        <location filename="../minstall.cpp" line="1219"/>
        <source>&lt;p&gt;&lt;b&gt;Support %1&lt;/b&gt;&lt;br/&gt;%1 is supported by people like you. Some help others at the support forum - %2 - or translate help files into different languages, or make suggestions, write documentation, or help test new software.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Apoie o %1&lt;/b&gt;&lt;br/&gt;O %1 é apoiado por pessoas como você. Alguns ajudam outras pessoas no fórum de suporte (%2), outras pessoas traduzem arquivos de ajuda para diferentes idiomas, ou fazem sugestões, elaboram documentação ou ajudam a testar os novos programas (software).&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="945"/>
        <source>Finish</source>
        <translation>Finalizar</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="948"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="950"/>
        <source>Next</source>
        <translation>Próximo</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="988"/>
        <source>Configuring sytem. Please wait.</source>
        <translation>Configurando o sistema operacional. Por favor, aguarde.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="992"/>
        <source>Configuration complete. Restarting system.</source>
        <translation>As configurações foram concluídas. Reiniciando o sistema operacional.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1015"/>
        <location filename="../minstall.cpp" line="1332"/>
        <source>Root</source>
        <translation>Root</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1017"/>
        <location filename="../minstall.cpp" line="1340"/>
        <source>Home</source>
        <translation>Home</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1154"/>
        <source>The installation and configuration is incomplete.
Do you really want to stop now?</source>
        <translation>A instalação e as configurações NÃO estão completas.
Você realmente quer parar agora?</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1205"/>
        <source>&lt;p&gt;&lt;b&gt;Getting Help&lt;/b&gt;&lt;br/&gt;Basic information about %1 is at %2.&lt;/p&gt;&lt;p&gt;There are volunteers to help you at the %3 forum, %4&lt;/p&gt;&lt;p&gt;If you ask for help, please remember to describe your problem and your computer in some detail. Usually statements like &apos;it didn&apos;t work&apos; are not helpful.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Obtenha Ajuda&lt;/b&gt;&lt;br/&gt;Informações básicas sobre o %1 estão disponíveis em %2.&lt;/p&gt;&lt;p&gt;Há voluntários que prestam ajuda no fórum do %3, %4 (maioritariamente em idioma inglês)&lt;/p&gt;&lt;p&gt;Ao solicitar ajuda, lembre-se de descrever o seu problema e o seu computador com detalhes. Normalmente, afirmações como &apos;(algo) não funcionou&apos; não ajudam a proporcionar uma boa ajuda.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1213"/>
        <source>&lt;p&gt;&lt;b&gt;Repairing Your Installation&lt;/b&gt;&lt;br/&gt;If %1 stops working from the hard drive, sometimes it&apos;s possible to fix the problem by booting from LiveDVD or LiveUSB and running one of the included utilities in %1 or by using one of the regular Linux tools to repair the system.&lt;/p&gt;&lt;p&gt;You can also use your %1 LiveDVD or LiveUSB to recover data from MS-Windows systems!&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Reparando o Sistema Operacional&lt;/b&gt;&lt;br/&gt;Se o %1 parar de funcionar no disco rígido, às vezes é possível corrigir o problema inicializando a partir de um dispositivo externo executável CD/DVD ou USB, executando um dos utilitários incluídos no %1 ou utilizando uma das ferramentas padrões do GNU/Linux para reparar o sistema operacional.&lt;/p&gt;&lt;p&gt;O %1 em CD/DVD executável ou em um USB executável (Live USB), pode ser utilizado para recuperar dados em sistemas operacionais Windows da Microsoft!&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1227"/>
        <source>&lt;p&gt;&lt;b&gt;Adjusting Your Sound Mixer&lt;/b&gt;&lt;br/&gt; %1 attempts to configure the sound mixer for you but sometimes it will be necessary for you to turn up volumes and unmute channels in the mixer in order to hear sound.&lt;/p&gt; &lt;p&gt;The mixer shortcut is located in the menu. Click on it to open the mixer. &lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Ajustando o Misturador de Som&lt;/b&gt;&lt;br/&gt;O %1 tenta configurar o misturador de som (sound mixer), mas por vezes é necessário aumentar o volume ou desativar a função &apos;mute&apos; (mudo) nos canais do misturador para que se possa ouvir o som. &lt;/p&gt;&lt;p&gt;O atalho para o misturador está localizado no menu. Clique para abrir o misturador.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1235"/>
        <source>&lt;p&gt;&lt;b&gt;Keep Your Copy of %1 up-to-date&lt;/b&gt;&lt;br/&gt;For more information and updates please visit&lt;/p&gt;&lt;p&gt; %2&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Mantenha o Seu %1 Atualizado&lt;/b&gt;&lt;br/&gt;Para mais informações e atualizações do %1, por favor, visite&lt;/p&gt;&lt;p&gt; %2&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1240"/>
        <source>&lt;p&gt;&lt;b&gt;Special Thanks&lt;/b&gt;&lt;br/&gt;Thanks to everyone who has chosen to support %1 with their time, money, suggestions, work, praise, ideas, promotion, and/or encouragement.&lt;/p&gt;&lt;p&gt;Without you there would be no %1.&lt;/p&gt;&lt;p&gt;%2 Dev Team&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Agradecimentos Especiais&lt;/b&gt;&lt;br/&gt;Agradecemos a todos os que decidiram apoiar o %1 com o seu tempo, os seus donativos, com as suas sugestões, o seu trabalho, os seus elogios, as suas ideias, promovendo-o e/ou encorajando-nos.&lt;/p&gt;&lt;p&gt;Sem vocês o %1 não existiria.&lt;/p&gt;&lt;p&gt;A equipe de desenvolvimento do %2&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1291"/>
        <source>%1% root
%2% home</source>
        <translation>%1% root
%2% home</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1293"/>
        <source>Combined root and home</source>
        <translation>O root e a home combinados</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1337"/>
        <source>----</source>
        <translation>----</translation>
    </message>
</context>
<context>
    <name>MPassEdit</name>
    <message>
        <location filename="../mpassedit.cpp" line="101"/>
        <source>Use password</source>
        <translation>Utilizar a senha</translation>
    </message>
    <message>
        <location filename="../mpassedit.cpp" line="188"/>
        <source>Hide the password</source>
        <translation>Ocultar a senha</translation>
    </message>
    <message>
        <location filename="../mpassedit.cpp" line="188"/>
        <source>Show the password</source>
        <translation>Exibir a senha</translation>
    </message>
</context>
<context>
    <name>MeInstall</name>
    <message>
        <location filename="../meinstall.ui" line="43"/>
        <source>Help</source>
        <translation>Ajuda</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="69"/>
        <source>Live Log</source>
        <translation>Arquivo de Registro</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="107"/>
        <source>Close</source>
        <translation>Fechar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="118"/>
        <source>Next</source>
        <translation>Próximo</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="125"/>
        <source>Alt+N</source>
        <translation>Alt+N</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="166"/>
        <source>Gathering Information, please stand by.</source>
        <translation>Por favor, aguarde a coleta de informações.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="201"/>
        <source>Terms of Use</source>
        <translation>Termos de Utilização</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="228"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;center&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Keyboard Settings&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;center&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Configurações do Teclado&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="235"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;right&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Model:&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;right&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Modelo:&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="242"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;right&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Variant:&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;right&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Variante:&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="259"/>
        <source>Change Keyboard Settings</source>
        <translation>Alterar as Configurações do Teclado</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="266"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;right&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Layout:&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;right&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Leiaute:&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="317"/>
        <source>Select type of installation</source>
        <translation>Selecionar o tipo de instalação</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="329"/>
        <source>Regular install using the entire disk</source>
        <translation>Instalação regular utilizando todo o disco</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="342"/>
        <source>Customize the disk layout</source>
        <translation>Personalizar o leiaute do disco</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="355"/>
        <source>Use disk:</source>
        <translation>Utilizar o disco:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="368"/>
        <source>Encrypt</source>
        <translation>Criptografar/Encriptar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="380"/>
        <location filename="../meinstall.ui" line="669"/>
        <source>Encryption password:</source>
        <translation>Senha de criptografia/encriptação</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="410"/>
        <location filename="../meinstall.ui" line="702"/>
        <source>Confirm password:</source>
        <translation>Confirmar a senha:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="457"/>
        <source>Root</source>
        <translation>Root</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="526"/>
        <source>Home</source>
        <translation>Home</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="567"/>
        <source>Choose partitions</source>
        <translation>Escolher as partições</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="576"/>
        <source>Query the operating system and reload the layouts of all drives.</source>
        <translation>Consulte o sistema operacional e recarregue os leiautes de todas as unidades.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="587"/>
        <source>Remove an existing entry from the layout. This only works with entries to a new layout.</source>
        <translation>Remover uma entrada existente do leiaute. Isto só funciona com entradas para um novo leiaute.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="598"/>
        <source>Add a new partition entry. This only works with a new layout.</source>
        <translation>Adicione uma nova entrada de partição. Isto só funciona com um novo leiaute.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="609"/>
        <source>Mark the selected drive to be cleared for a new layout.</source>
        <translation>Marque a unidade selecionada para ser limpa para um novo leiaute.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="630"/>
        <source>Run the partition management application of this operating system.</source>
        <translation>Execute o aplicativo o gerenciador de partição GParted deste sistema operacional.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="653"/>
        <source>Encryption options</source>
        <translation>Opções de criptografia/encriptação</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="728"/>
        <source>Install GRUB for Linux and Windows</source>
        <translation>Instalar o GRUB para o GNU/Linux e o Windows</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="752"/>
        <source>Master Boot Record</source>
        <translation>Master Boot Record</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="758"/>
        <source>MBR</source>
        <translation>MBR</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="761"/>
        <source>Alt+B</source>
        <translation>Alt+B</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="783"/>
        <source>EFI System Partition</source>
        <translation>Partição de Sistema EFI</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="786"/>
        <source>ESP</source>
        <translation>ESP</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="812"/>
        <source>Partition Boot Record</source>
        <translation>Partition Boot Record</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="815"/>
        <source>PBR</source>
        <translation>PBR</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="828"/>
        <source>System boot disk:</source>
        <translation>Disco de inicialização do sistema:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="841"/>
        <source>Location to install on:</source>
        <translation>Local para instalar:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="871"/>
        <source>Create a swap file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="886"/>
        <source>Location:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="893"/>
        <source>Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="915"/>
        <source> MB</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="964"/>
        <source>Common Services to Enable</source>
        <translation>Ativar os Serviços Comuns</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="983"/>
        <source>Service</source>
        <translation>Serviço</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="988"/>
        <source>Description</source>
        <translation>Descrição</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1021"/>
        <source>Computer Network Names</source>
        <translation>Nomes dos Computadores na Rede</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1048"/>
        <source>Workgroup</source>
        <translation>Grupo de trabalho</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1061"/>
        <source>Workgroup:</source>
        <translation>Grupo de trabalho:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1074"/>
        <source>SaMBa Server for MS Networking</source>
        <translation>Servidor de arquivos SaMBa para interação em rede com sistemas MS Windows</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1090"/>
        <source>example.dom</source>
        <translation>exemplo.dom</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1103"/>
        <source>Computer domain:</source>
        <translation>Domínio do computador:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1129"/>
        <source>Computer name:</source>
        <translation>Nome do computador:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1184"/>
        <source>Configure Clock</source>
        <translation>Configurar o Relógio</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1219"/>
        <source>Format:</source>
        <translation>Formato:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1247"/>
        <source>Timezone:</source>
        <translation>Fuso Horário:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1286"/>
        <source>System clock uses local time</source>
        <translation>O relógio do sistema utiliza a hora local</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1312"/>
        <source>Localization Defaults</source>
        <translation>Padrões de Localização</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1352"/>
        <source>Locale:</source>
        <translation>Localização:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1374"/>
        <source>Service Settings (advanced)</source>
        <translation>Configurações de Serviços (avançados)</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1392"/>
        <source>Adjust which services should run at startup</source>
        <translation>Definir quais serviços devem ser executados na inicialização</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1395"/>
        <source>View</source>
        <translation>Exibir</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1434"/>
        <source>Desktop modifications made in the live environment will be carried over to the installed OS</source>
        <translation>As alterações da área de trabalho feitas na sessão da instalação externa serão transpostas para o sistema instalado no disco rígido</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1437"/>
        <source>Save live desktop changes</source>
        <translation>Salvar as alterações feitas na área de trabalho da sessão da instalação externa</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1450"/>
        <source>Default User Account</source>
        <translation>Conta de Usuário Padrão</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1462"/>
        <source>Default user login name:</source>
        <translation>Nome de acesso (login) do usuário padrão:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1478"/>
        <source>Default user password:</source>
        <translation>Senha de usuário padrão:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1491"/>
        <source>Confirm user password:</source>
        <translation>Confirmar a senha de usuário:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1552"/>
        <source>username</source>
        <translation>nomedeusuario</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1568"/>
        <source>Root (administrator) Account</source>
        <translation>Conta de Root (Administrador)</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1586"/>
        <source>Root password:</source>
        <translation>Senha de root:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1631"/>
        <source>Confirm root password:</source>
        <translation>Confirmar a senha de root:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1660"/>
        <source>Autologin</source>
        <translation>Iniciar a sessão sem autenticar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1690"/>
        <source>Existing Home Directory</source>
        <translation>Pasta Home Existente</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1699"/>
        <source>What would you like to do with the old directory?</source>
        <translation>O que você quer fazer com a pasta pessoal (diretório pessoal) antiga?</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1706"/>
        <source>Re-use it for this installation</source>
        <translation>Reutilizar para esta instalação</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1713"/>
        <source>Rename it and create a new directory</source>
        <translation>Renomear e criar uma nova pasta pessoal (diretório pessoal)</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1720"/>
        <source>Delete it and create a new directory</source>
        <translation>Apagar e criar uma nova pasta pessoal (diretório pessoal)</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1759"/>
        <source>Tips</source>
        <translation>Dicas</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1803"/>
        <source>Installation complete</source>
        <translation>A instalação foi concluída</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1809"/>
        <source>Automatically reboot the system when the installer is closed</source>
        <translation>Reinicializar automaticamente o sistema operacional quando o instalador for fechado</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1828"/>
        <source>Reminders</source>
        <translation>Lembretes</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1863"/>
        <source>Back</source>
        <translation>Voltar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1870"/>
        <source>Alt+K</source>
        <translation>Alt+K</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1883"/>
        <source>Installation in progress</source>
        <translation>A instalação está em andamento</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1898"/>
        <source>Abort</source>
        <translation>Abortar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1901"/>
        <source>Alt+A</source>
        <translation>Alt+A</translation>
    </message>
</context>
<context>
    <name>Oobe</name>
    <message>
        <location filename="../oobe.cpp" line="321"/>
        <source>Please enter a computer name.</source>
        <translation>Por favor, insira o nome do computador</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="325"/>
        <source>Sorry, your computer name contains invalid characters.
You&apos;ll have to select a different
name before proceeding.</source>
        <translation>Desculpe, o nome do seu computador contém
caracteres inválidos. Você tem que escolher um
nome  diferente antes de prosseguir.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="330"/>
        <source>Please enter a domain name.</source>
        <translation>Por favor, insira um nome de domínio</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="334"/>
        <source>Sorry, your computer domain contains invalid characters.
You&apos;ll have to select a different
name before proceeding.</source>
        <translation>Desculpe, o nome de domínio do seu computador
contém caracteres inválidos. Você tem que
escolher um nome diferente antes de prosseguir.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="341"/>
        <source>Please enter a workgroup.</source>
        <translation>Por favor, insira um grupo de trabalho</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="481"/>
        <source>The user name cannot contain special characters or spaces.
Please choose another name before proceeding.</source>
        <translation>O nome de usuário não pode conter caracteres especiais ou espaços.
Por favor, insira outro nome de usuário antes de prosseguir.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="492"/>
        <source>Sorry, that name is in use.
Please select a different name.</source>
        <translation>Este nome já está em uso.
Por favor, insira um nome diferente.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="501"/>
        <source>You did not provide a password for the root account. Do you want to continue?</source>
        <translation>Você não forneceu uma senha para a conta de root (administrador). Você quer continuar?</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="530"/>
        <source>Failed to set user account passwords.</source>
        <translation>Ocorreu uma falha ao definir as senhas para a conta do usuário.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="549"/>
        <source>Failed to save old home directory.</source>
        <translation>Ocorreu uma falha ao guardar a pasta home antiga.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="553"/>
        <source>Failed to delete old home directory.</source>
        <translation>Ocorreu uma falha ao apagar a pasta home antiga.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="574"/>
        <source>Sorry, failed to create user directory.</source>
        <translation>Desculpe, ocorreu uma falha ao criar a pasta pessoal (diretório pessoal) do usuário</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="578"/>
        <source>Sorry, failed to name user directory.</source>
        <translation>Desculpe, ocorreu uma falha ao nomear a pasta pessoal (diretório pessoal) do usuário.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="620"/>
        <source>Failed to set ownership or permissions of user directory.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Sorry, failed to set ownership of user directory.</source>
        <translation type="vanished">Desculpe, não foi possível atribuir a propriedade da pasta pessoal (diretório pessoal) do usuário.</translation>
    </message>
</context>
<context>
    <name>PartMan</name>
    <message>
        <location filename="../partman.cpp" line="226"/>
        <source>Virtual Devices</source>
        <translation>Dispositivos Virtuais</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="450"/>
        <location filename="../partman.cpp" line="504"/>
        <source>&amp;Add partition</source>
        <translation>&amp;Adicionar a partição</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="452"/>
        <source>&amp;Remove partition</source>
        <translation>&amp;Remover a partição</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="462"/>
        <source>&amp;Lock</source>
        <translation>&amp;Bloquear</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="466"/>
        <source>&amp;Unlock</source>
        <translation>&amp;Desbloquear</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="471"/>
        <location filename="../partman.cpp" line="601"/>
        <source>Add to crypttab</source>
        <translation>Adicionar o crypttab (tabela de dispositivos criptografados)</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="477"/>
        <source>Active partition</source>
        <translation>Partição Ativa</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="483"/>
        <source>New subvolume</source>
        <translation>Novo subvolume</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="484"/>
        <source>Scan subvolumes</source>
        <translation>Verificar os subvolumes</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="507"/>
        <source>New &amp;layout</source>
        <translation>Novo &amp;leiaute</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="508"/>
        <source>&amp;Reset layout</source>
        <translation>&amp;Redefinir o leiaute</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="509"/>
        <source>&amp;Templates</source>
        <translation>&amp;Modelos</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="510"/>
        <source>&amp;Standard install</source>
        <translation>&amp;Instalação padrão</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="511"/>
        <source>&amp;Encrypted system</source>
        <translation>%Sistema criptografado/encriptado</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="530"/>
        <source>Remove subvolume</source>
        <translation>Remover o subvolume</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="598"/>
        <source>Unlock Drive</source>
        <translation>Desbloquear o Dispositivo</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="604"/>
        <source>Virtual Device:</source>
        <translation>Dispositivo Virtual:</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="605"/>
        <source>Password:</source>
        <translation>Senha:</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="634"/>
        <source>Could not unlock device. Possible incorrect password.</source>
        <translation>Não foi possível desbloquear o dispositivo. A senha pode estar incorreta.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="663"/>
        <source>Failed to close %1</source>
        <translation>Ocorreu uma falha ao fechar %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="707"/>
        <source>Invalid subvolume label</source>
        <translation>O rótulo do subvolume não é válido</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="718"/>
        <source>Duplicate subvolume label</source>
        <translation>O rótulo do subvolume está duplicado</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="727"/>
        <source>Invalid use for %1: %2</source>
        <translation>Uso inválido para %1: %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="738"/>
        <source>%1 is already selected for: %2</source>
        <translation>%1 já está selecionado para: %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="763"/>
        <source>A root partition of at least %1 is required.</source>
        <translation>É necessária uma partição de root (raiz) de pelo menos %1.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="767"/>
        <source>Cannot preserve /home inside root (/) if a separate /home partition is also mounted.</source>
        <translation>Não é possível preservar /home dentro do root (/) se uma partição /home separada também estiver montada.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="776"/>
        <source>You must choose a separate boot partition when encrypting root.</source>
        <translation>Você tem que escolher uma partição de inicialização (boot) diferente quando a partição raiz (root) for criptografada/encriptada.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="788"/>
        <source>Prepare %1 partition table on %2</source>
        <translation>Preparar a tabela de partição %1 em %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="800"/>
        <source>Format %1 to use for %2</source>
        <translation>Formatar %1 para utilizar %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="801"/>
        <source>Reuse (no reformat) %1 as %2</source>
        <translation>Reutilizar (sem reformatar) %1 como %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="802"/>
        <source>Delete the data on %1 except for /home, to use for %2</source>
        <translation>Apagar os dados em %1 exceto em /home, para utilizar em %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="804"/>
        <source>Create %1 without formatting</source>
        <translation>Cria %1 sem formatação</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="805"/>
        <source>Create %1, format to use for %2</source>
        <translation>Criar %1, o formato a ser utilizado para %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="822"/>
        <source>The following drives are, or will be, setup with GPT, but do not have a BIOS-GRUB partition:</source>
        <translation>As seguintes unidades são, ou serão, configuradas com GPT, mas não têm uma partição BIOS-GRUB:</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="824"/>
        <source>This system may not boot from GPT drives without a BIOS-GRUB partition.</source>
        <translation>Este sistema não pode inicializar a partir de unidades GPT sem uma partição BIOS-GRUB.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="825"/>
        <source>Are you sure you want to continue?</source>
        <translation>Você tem certeza de que quer continuar?</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="829"/>
        <source>The %1 installer will now perform the requested actions.</source>
        <translation>O instalador do %1 agora executará as ações solicitadas.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="830"/>
        <source>These actions cannot be undone. Do you want to continue?</source>
        <translation>Estas ações não poderão ser desfeitas. Você quer continuar?</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="869"/>
        <source>The disks with the partitions you selected for installation are failing:</source>
        <translation>Os discos com as partições selecionadas para a instalação estão falhando:</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="873"/>
        <source>Smartmon tool output:</source>
        <translation>Resultados da ferramenta Smartmon:</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="874"/>
        <source>The disks with the partitions you selected for installation pass the SMART monitor test (smartctl), but the tests indicate it will have a higher than average failure rate in the near future.</source>
        <translation>Os discos com as partições selecionadas para a instalação passaram no teste de monitorização SMART (smartctl), mas os testes indicam que terão uma taxa de falha superior à média em um futuro próximo.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="879"/>
        <source>If unsure, please exit the Installer and run GSmartControl for more information.</source>
        <translation>Se você não tem certeza, saia do instalador e execute o GSmartControl para obter mais informações.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="881"/>
        <source>Do you want to abort the installation?</source>
        <translation>Você quer abortar a instalação?</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="886"/>
        <source>Do you want to continue?</source>
        <translation>Você quer continuar?</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="901"/>
        <source>Failed to format LUKS container.</source>
        <translation>Ocorreu uma falha ao formatar o contêiner LUKS.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="910"/>
        <source>Failed to open LUKS container.</source>
        <translation>Ocorreu uma falha ao abrir o contêiner LUKS.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="960"/>
        <source>Failed to prepare required partitions.</source>
        <translation>Ocorreu uma falha ao preparar as partições requeridas.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="991"/>
        <source>Preparing partition tables</source>
        <translation>Preparando as tabelas de partição</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1007"/>
        <source>Preparing required partitions</source>
        <translation>Preparando as partições solicitadas</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1073"/>
        <source>Creating encrypted volume: %1</source>
        <translation>Criando o volume criptografado/encriptado: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1078"/>
        <source>Formatting: %1</source>
        <translation>Formatando: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1081"/>
        <source>Failed to format partition.</source>
        <translation>Ocorreu uma falha ao formatar a partição.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1139"/>
        <source>Failed to prepare subvolumes.</source>
        <translation>Ocorreu uma falha ao preparar os subvolumes.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1148"/>
        <source>Preparing subvolumes</source>
        <translation>Preparando os subvolumes</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1321"/>
        <source>Failed to mount partition.</source>
        <translation>Ocorreu uma falha ao montar a partição.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1324"/>
        <source>Mounting: %1</source>
        <translation>Montagem: % 1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1484"/>
        <source>Model: %1</source>
        <translation>Modelo: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1492"/>
        <source>Free space: %1</source>
        <translation>Espaço livre: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1577"/>
        <source>Device</source>
        <translation>Dispositivo</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1578"/>
        <source>Size</source>
        <translation>Tamanho</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1579"/>
        <source>Use For</source>
        <translation>Para a Utilização</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1580"/>
        <source>Label</source>
        <translation>Rótulo</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1581"/>
        <source>Encrypt</source>
        <translation>Criptografar/Encriptar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1582"/>
        <source>Format</source>
        <translation>Formato</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1583"/>
        <source>Check</source>
        <translation>Verificar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1584"/>
        <source>Options</source>
        <translation>Opções</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1585"/>
        <source>Dump</source>
        <translation>Despejar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1586"/>
        <source>Pass</source>
        <translation>Passar</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../app.cpp" line="63"/>
        <source>Customizable GUI installer for MX Linux and antiX Linux</source>
        <translation>Instalador GUI (Interface Gráfica do Usuário) personalizável para o antiX Linux e o MX Linux</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="66"/>
        <source>Installs automatically using the configuration file (more information below).
-- WARNING: potentially dangerous option, it will wipe the partition(s) automatically.</source>
        <translation>Instala automaticamente utilizando o arquivo de configurações (obtenha mais informações abaixo) .
- AVISO: esta opção é potencialmente perigosa, limpará (apagará) a(s) partição(ões) automaticamente.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="68"/>
        <source>Overrules sanity checks on partitions and drives, causing them to be displayed.
-- WARNING: this can break things, use it only if you don&apos;t care about data on drive.</source>
        <translation>Anula as verificações de integridade em partições e unidades, fazendo com que sejam exibidas.
- ATENÇÃO: esta opção pode danificar os seus dados, utilize-a somente se você não se importar com os dados contidos na unidade.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="70"/>
        <source>Load a configuration file as specified by &lt;config-file&gt;.
By default /etc/minstall.conf is used.
This configuration can be used with --auto for an unattended installation.
The installer creates (or overwrites) /mnt/antiX/etc/minstall.conf and saves a copy to /etc/minstalled.conf for future use.
The installer will not write any passwords or ignored settings to the new configuration file.
Please note, this is experimental. Future installer versions may break compatibility with existing configuration files.</source>
        <translation>Carregue um arquivo de configurações conforme especificado por &lt;config-file&gt;.
Por padrão /etc/minstall.conf é utilizado.
Estas configurações podem ser utilizadas com --auto para uma instalação autônoma.
O instalador cria (ou sobrescreve) /mnt/antiX/etc/minstall.conf e salva uma cópia em /etc/minstalled.conf para a utilização futura.
O instalador não gravará nenhuma senha ou configurações ignoradas no novo arquivo de configurações.
Por favor, observe que isto é experimental. As versões futuras do instalador podem quebrar a compatibilidade com os arquivos de configurações existentes.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="76"/>
        <source>Shutdowns automatically when done installing.</source>
        <translation>Desligar automaticamente o computador quando finalizar a instalação.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="77"/>
        <source>Always use GPT when doing a whole-drive installation regardlesss of capacity.
Without this option, GPT will only be used on drives with at least 2TB capacity.
GPT is always used on whole-drive installations on UEFI systems regardless of capacity, even without this option.</source>
        <translation>Utilize sempre o GPT ao fazer uma instalação de unidade (disco rígido ou dispositivo de armazenamento) inteira, independentemente da capacidade de armazenamento.
Sem esta opção, o GPT só será utilizado em unidades com pelo menos 2 TB de capacidade.
O GPT é sempre utilizado em instalações de unidade inteira em sistemas UEFI, independentemente da capacidade de armazenamento, mesmo sem esta opção.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="80"/>
        <source>Do not unmount /mnt/antiX or close any of the associated LUKS containers when finished.</source>
        <translation>Não desmonte /mnt/antiX ou feche qualquer um dos contêineres LUKS associados quando finalizar.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="81"/>
        <source>Another testing mode for installer, partitions/drives are going to be FORMATED, it will skip copying the files.</source>
        <translation>Outro modo de teste para o instalador, partições/discos serão FORMATADOS, ele irá ignorar a cópia dos arquivos.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="82"/>
        <source>Install the operating system, delaying prompts for user-specific options until the first reboot.
Upon rebooting, the installer will be run with --oobe so that the user can provide these details.
This is useful for OEM installations, selling or giving away a computer with an OS pre-loaded on it.</source>
        <translation>Instale o sistema operacional, atrasando os avisos (prompts) das opções específicas do usuário até a primeira reinicialização.
Após a reinicialização, o instalador será executado com --oobe para que o usuário possa fornecer estas informações.
Isto é útil para as instalações OEM (Original Equipment Manufacturer - Fabricante de Equipamento Original), quando um computador é vendido ou distribuído com um sistema operacional pré-instalado no equipamento.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="85"/>
        <source>Out Of the Box Experience option.
This will start automatically if installed with --oem option.</source>
        <translation>Opção de Experiência Fora da Caixa (Out Of the Box).
Isto iniciará automaticamente se for instalado com a opção --oem.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="87"/>
        <source>Test mode for GUI, you can advance to different screens without actially installing.</source>
        <translation>Modo de teste da Interface Gráfica do Usuário - GUI, você pode avançar para telas diferentes sem realmente instalar.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="88"/>
        <source>Reboots automatically when done installing.</source>
        <translation>Reiniciar automaticamente o computador quando finalizar a instalação.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="89"/>
        <source>Installing with rsync instead of cp on custom partitioning.
-- doesn&apos;t format /root and it doesn&apos;t work with encryption.</source>
        <translation>Instalando com rsync em vez do cp no particionamento personalizado.
-- não formata /root e não funciona com criptografia/encriptação.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="91"/>
        <source>Always check the installation media at the beginning.</source>
        <translation>Sempre verifique a mídia de instalação no início.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="92"/>
        <source>Do not check the installation media at the beginning.
Not recommended unless the installation media is guaranteed to be free from errors.</source>
        <translation>Não verifique a mídia de instalação no início.
Não é recomendado, a menos que a mídia de instalação esteja livre de erros ou falhas.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="94"/>
        <source>Load a configuration file as specified by &lt;config-file&gt;.</source>
        <translation>Carregue um arquivo de configurações conforme especificado por &lt;config-file&gt;.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="98"/>
        <source>Too many arguments. Please check the command format by running the program with --help</source>
        <translation>Existem muitos argumentos. Por favor, verifique o formato do comando executando o programa com o parâmetro --help</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="103"/>
        <source>%1 Installer</source>
        <translation>Instalador do %1</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="111"/>
        <source>The installer won&apos;t launch because it appears to be running already in the background.

Please close it if possible, or run &apos;pkill minstall&apos; in terminal.</source>
        <translation>O instalador não pode iniciar porque parece que já está sendo executado em segundo plano.

Por favor, feche-o se for possível; se não, execute no terminal/console o comando &apos;pkill minstall&apos;.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="118"/>
        <source>This operation requires root access.</source>
        <translation>Esta operação requer o acesso de root (administrador).</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="139"/>
        <source>Configuration file (%1) not found.</source>
        <translation>O arquivo de configurações (%1) não foi encontrado.</translation>
    </message>
</context>
<context>
    <name>SwapMan</name>
    <message>
        <location filename="../swapman.cpp" line="55"/>
        <source>Failed to create or install swap file.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../swapman.cpp" line="59"/>
        <source>Creating swap file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../swapman.cpp" line="66"/>
        <source>Configuring swap file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../swapman.cpp" line="102"/>
        <source>Invalid location</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../swapman.cpp" line="105"/>
        <source>Maximum: %1 MB</source>
        <translation type="unfinished"></translation>
    </message>
</context>
</TS>
