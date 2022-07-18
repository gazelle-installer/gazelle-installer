<?xml version="1.0" ?><!DOCTYPE TS><TS version="2.1" language="pt">
<context>
    <name>Base</name>
    <message>
        <location filename="../base.cpp" line="85"/>
        <source>Deleting old system</source>
        <translation>A remover o sistema antigo</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="89"/>
        <source>Failed to delete old system on destination.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../base.cpp" line="95"/>
        <source>Creating system directories</source>
        <translation>A criar os diretórios (pastas) do sistema</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="105"/>
        <source>Fixing configuration</source>
        <translation>A reparar a configuração</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="117"/>
        <source>Failed to finalize encryption setup.</source>
        <translation>Falha ao finalizar a configuração de encriptação.</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="186"/>
        <source>Copying new system</source>
        <translation>A gravar o novo sistema</translation>
    </message>
    <message>
        <location filename="../base.cpp" line="217"/>
        <source>Failed to copy the new system.</source>
        <translation type="unfinished"/>
    </message>
</context>
<context>
    <name>BootMan</name>
    <message>
        <location filename="../bootman.cpp" line="124"/>
        <location filename="../bootman.cpp" line="233"/>
        <source>Updating initramfs</source>
        <translation>A actualizar o initramfs</translation>
    </message>
    <message>
        <location filename="../bootman.cpp" line="132"/>
        <source>Failed to update initramfs.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../bootman.cpp" line="136"/>
        <source>Installing GRUB</source>
        <translation>A instalar o GRUB</translation>
    </message>
    <message>
        <location filename="../bootman.cpp" line="173"/>
        <source>GRUB installation failed. You can reboot to the live medium and use the GRUB Rescue menu to repair the installation.</source>
        <translation>A instalação do GRUB falhou. É possível reparar a instalação do GRUB reiniciando o computador com o sistema externo e usando o menu &apos;Recuperação do GRUB&apos;.</translation>
    </message>
    <message>
        <location filename="../bootman.cpp" line="262"/>
        <source>System boot disk:</source>
        <translation>Disco de arranque do sistema:</translation>
    </message>
    <message>
        <location filename="../bootman.cpp" line="281"/>
        <location filename="../bootman.cpp" line="293"/>
        <source>Partition to use:</source>
        <translation>Partição a usar:</translation>
    </message>
</context>
<context>
    <name>DeviceItem</name>
    <message>
        <location filename="../partman.cpp" line="1791"/>
        <source>EFI System Partition</source>
        <translation>Partição de Sistema EFI</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1792"/>
        <source>swap space</source>
        <translation>espaço swap</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1793"/>
        <source>format only</source>
        <translation>apenas formatar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1916"/>
        <source>Create</source>
        <translation>Criar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1919"/>
        <source>Preserve</source>
        <translation>Preservar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1920"/>
        <source>Preserve (%1)</source>
        <translation>Preservar (%1)</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1921"/>
        <source>Preserve /home (%1)</source>
        <translation>Preservar /home (%1)</translation>
    </message>
</context>
<context>
    <name>DeviceItemDelegate</name>
    <message>
        <location filename="../partman.cpp" line="2357"/>
        <source>&amp;Templates</source>
        <translation>&amp;Modelos</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="2364"/>
        <source>Compression (&amp;ZLIB)</source>
        <translation>Compressão (&amp;ZLIB)</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="2366"/>
        <source>Compression (Z&amp;STD)</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../partman.cpp" line="2368"/>
        <source>Compression (&amp;LZO)</source>
        <translation>Compressão (&amp;LZO)</translation>
    </message>
</context>
<context>
    <name>MInstall</name>
    <message>
        <location filename="../minstall.cpp" line="68"/>
        <source>Shutdown</source>
        <translation>Desligar</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="128"/>
        <source>You are running 32bit OS started in 64 bit UEFI mode, the system will not be able to boot unless you select Legacy Boot or similar at restart.
We recommend you quit now and restart in Legacy Boot

Do you want to continue the installation?</source>
        <translation>O Sistema Operativo de 32bit em execução foi iniciado em modo UEFI 64 bit. Depois de instalado no disco, o sistema não arrancará a menos que no reinício seja seleccionado o modo Legacy Boot - isto é, o modo BIOS - ou similar.
Recomenda-se que o sistema seja encerrado agora e reiniciado em modo Legacy Boot.

Continuar a instalação, ainda assim?</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="145"/>
        <source>Cannot access installation source.</source>
        <translation>Impossível aceder à fonte da instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="162"/>
        <source>Support %1

%1 is supported by people like you. Some help others at the support forum - %2, or translate help files into different languages, or make suggestions, write documentation, or help test new software.</source>
        <translation>Apoiar o %1

O %1 é apoiado por pessoas como você. Algumas ajudam outras no fórum de apoio -%2, traduzem ficheiros de ajuda para diferentes idiomas, fazem sugestões, redigem documentação ou ajudam a testar novo software.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="212"/>
        <source>%1 is an independent Linux distribution based on Debian Stable.

%1 uses some components from MEPIS Linux which are released under an Apache free license. Some MEPIS components have been modified for %1.

Enjoy using %1</source>
        <translation>O %1 é uma distribuição Linux independente baseada na distribuição Debian Stable.

O %1 usa alguns componentes do MEPIS Linux publicados sob Licença Apache (Apache free license). Alguns componentes do MEPIS foram modificados para o %1.

Usufrua do %1.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="348"/>
        <source>Pretending to install %1</source>
        <translation>Simulando a instalação do %1</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="368"/>
        <source>Preparing to install %1</source>
        <translation>A preparar para instalar o %1</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="390"/>
        <source>Paused for required operator input</source>
        <translation>A aguardar que o operador introduza informação requerida</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="401"/>
        <source>Setting system configuration</source>
        <translation>A estabelecar a configuração do sistema</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="414"/>
        <source>Cleaning up</source>
        <translation>A limpar</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="416"/>
        <source>Finished</source>
        <translation>Terminado</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="511"/>
        <source>Invalid settings found in configuration file (%1). Please review marked fields as you encounter them.</source>
        <translation>Definições inválidas encontradas no ficheiro de configuração (%1). Rever os campos marcados quando forem encontrados.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="552"/>
        <source>OK to format and use the entire disk (%1) for %2?</source>
        <translation>Formatar e usar todo o disco (%1) para o %2?</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="556"/>
        <source>WARNING: The selected drive has a capacity of at least 2TB and must be formatted using GPT. On some systems, a GPT-formatted disk will not boot.</source>
        <translation>AVISO: o disco seleccionado tem a capacidade de pelo menos 2TB e tem que ser formatado em GPT. Em alguns sistemas, um disco formatado em GPT não arrancará.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="582"/>
        <source>The data in /home cannot be preserved because the required information could not be obtained.</source>
        <translation>Os dados em /home não podem ser presevados porque a informação requerida para esse processo não pôde ser obtida.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="597"/>
        <source>The home directory for %1 already exists.</source>
        <translation>A pasta pessoal para %1 já existe.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="644"/>
        <source>General Instructions</source>
        <translation>Instruções gerais</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="645"/>
        <source>BEFORE PROCEEDING, CLOSE ALL OTHER APPLICATIONS.</source>
        <translation>ANTES DE PROSSEGUIR, FECHAR TODAS AS OUTRAS APLICAÇÕES.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="646"/>
        <source>On each page, please read the instructions, make your selections, and then click on Next when you are ready to proceed. You will be prompted for confirmation before any destructive actions are performed.</source>
        <translation>Em cada página, ler as instruções, fazer as escolhas requeridas e clicar em Seguinte. Será pedida confirmação de continuação antes de quaisquer acções destrutivas serem executadas.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="648"/>
        <source>Limitations</source>
        <translation>Limitações</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="649"/>
        <source>Remember, this software is provided AS-IS with no warranty what-so-ever. It is solely your responsibility to backup your data before proceeding.</source>
        <translation>Ter presente: este software é disponibilizado COMO ESTÁ, sem qualquer tipo de garantia. É da exclusiva responsabilidade do utilizador fazer uma cópia de segurança dos dados que tenha no computador, antes de prosseguir.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="662"/>
        <source>Installation Options</source>
        <translation>Opções de instalação</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="663"/>
        <source>Installation requires about %1 of space. %2 or more is preferred.</source>
        <translation>A instalação requer cerca de %1 de espaço em disco. Mas é preferível usar %2 ou mais. </translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="664"/>
        <source>If you are running Mac OS or Windows OS (from Vista onwards), you may have to use that system&apos;s software to set up partitions and boot manager before installing.</source>
        <translation>Em computadores com sistema operativo Mac ou Windows (Vista ou posterior), poderá ser necessário usar o software do sistema existente para fazer partições e instalar o gestor de arranque (boot manager) antes de proceder à instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="665"/>
        <source>Using the root-home space slider</source>
        <translation>Uso do cursor de espaço root-home</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="666"/>
        <source>The drive can be divided into separate system (root) and user data (home) partitions using the slider.</source>
        <translation>Usando o cursor, a unidade pode ser dividida em duas partições, uma para o sistema (root) e outra para os dados do(s) utilizador(es) (home).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="667"/>
        <source>The &lt;b&gt;root&lt;/b&gt; partition will contain the operating system and applications.</source>
        <translation>A partição &lt;b&gt;root&lt;/b&gt; (raiz) é usada para o sistema operativo e para as aplicações.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="668"/>
        <source>The &lt;b&gt;home&lt;/b&gt; partition will contain the data of all users, such as their settings, files, documents, pictures, music, videos, etc.</source>
        <translation>A partição &lt;b&gt;home&lt;/b&gt; é usada para os dados dos utilizadores, em pastas pessoais (pastas de utilizador), isto é, configurações, ficheiros, documentos, imagens, músicas, vídeos, etc., próprios de cada um.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="669"/>
        <source>Move the slider to the right to increase the space for &lt;b&gt;root&lt;/b&gt;. Move it to the left to increase the space for &lt;b&gt;home&lt;/b&gt;.</source>
        <translation>Mover o cursor para  a direita para aumentar o espaço para &lt;b&gt; root &lt;/b&gt;. Mover para a esquerda para aumentar o espaço para &lt;b&gt; home &lt;/b&gt;. </translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="670"/>
        <source>Move the slider all the way to the right if you want both root and home on the same partition.</source>
        <translation>Mover o cursor totalmente para a direita para que root e home fiquem na mesma partição.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="671"/>
        <source>Keeping the home directory in a separate partition improves the reliability of operating system upgrades. It also makes backing up and recovery easier. This can also improve overall performance by constraining the system files to a defined portion of the drive.</source>
        <translation>Manter a pasta home numa partição separada aumenta a fiabilidade durante upgrades do sistema operativo. Também torna o processo de fazer e restaurar cópias de segurança mais fácil. Pode aumentar a performance geral ao limitar ficheiros do sistema a uma parte definida do disco.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="673"/>
        <location filename="../minstall.cpp" line="756"/>
        <source>Encryption</source>
        <translation>Encriptação</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="674"/>
        <location filename="../minstall.cpp" line="757"/>
        <source>Encryption is possible via LUKS. A password is required.</source>
        <translation>É possível encriptação via LUKS.  É necessária uma senha.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="675"/>
        <location filename="../minstall.cpp" line="758"/>
        <source>A separate unencrypted boot partition is required.</source>
        <translation>É necessária uma partição boot (de arranque) não encriptada.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="676"/>
        <source>When encryption is used with autoinstall, the separate boot partition will be automatically created.</source>
        <translation>Quando é feita uma instalação automática encriptada, é automaticamente criada uma partição de arranque (boot) separada.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="677"/>
        <source>Using a custom disk layout</source>
        <translation>Usar uma organização pessoal de disco</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="678"/>
        <source>If you need more control over where %1 is installed to, select &quot;&lt;b&gt;%2&lt;/b&gt;&quot; and click &lt;b&gt;Next&lt;/b&gt;. On the next page, you will then be able to select and configure the storage devices and partitions you need.</source>
        <translation>Para mais controlo sobre onde o %1 será instalado, seleccionar &quot;&lt;b&gt;%2&lt;/b&gt; e clicar em &lt;b&gt; Seguinte &lt;/b&gt;. Na página seguinte, poderão ser seleccionados e configurados os dispositivos de armazenamento e as partições como necessário.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="692"/>
        <source>Choose Partitions</source>
        <translation>Escolher Partições</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="693"/>
        <source>The partition list allows you to choose what partitions are used for this installation.</source>
        <translation>A lista de partições permite escolher as partições a usar para esta instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="694"/>
        <source>&lt;i&gt;Device&lt;/i&gt; - This is the block device name that is, or will be, assigned to the created partition.</source>
        <translation>&lt;i&gt; Dispositivo &lt;/i&gt; - Este é o nome de dispositivo de blocos que é, ou será, atribuído à partição criada.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="695"/>
        <source>&lt;i&gt;Size&lt;/i&gt; - The size of the partition. This can only be changed on a new layout.</source>
        <translation>&lt;i&gt; Tamanho &lt;/i&gt; - Tamanho da partição. Isto só pode ser alterado numa nova organização.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="696"/>
        <source>&lt;i&gt;Use For&lt;/i&gt; - To use this partition in an installation, you must select something here.</source>
        <translation>&lt;i&gt;Usar para&lt;/i&gt; - Para esta partição ser usada na instalação, tem que ser seleccionada uma opção aqui.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="697"/>
        <source>Format - Format without mounting.</source>
        <translation>Formatar - Formatar sem montar.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="698"/>
        <source>BIOS-GRUB - BIOS Boot GPT partition for GRUB.</source>
        <translation>BIOS-GRUB - Partição GPT de arranque por BIOS para o GRUB.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="699"/>
        <source>EFI - EFI System Partition.</source>
        <translation>EFI - Partição de Sistema EFI.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="700"/>
        <source>boot - Boot manager (/boot).</source>
        <translation>boot - Gestor do arranque (/boot).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="701"/>
        <source>root - System root (/).</source>
        <translation>root - Raiz do sistema ( / ).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="702"/>
        <source>swap - Swap space.</source>
        <translation>swap - Espaço para trocas.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="703"/>
        <source>home - User data (/home).</source>
        <translation>home - Ficheiros dos utilizadores (/home).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="704"/>
        <source>In addition to the above, you can also type your own mount point. Custom mount points must start with a slash (&quot;/&quot;).</source>
        <translation>Além do referido acima, também pode ser inserido um ponto de montagem próprio do utilizador. Os pontos de montagem têm que começar com uma barra (&quot;/&quot;).</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="705"/>
        <source>The installer treats &quot;/boot&quot;, &quot;/&quot;, and &quot;/home&quot; exactly the same as &quot;boot&quot;, &quot;root&quot;, and &quot;home&quot;, respectively.</source>
        <translation>O instalador trata &quot;/boot&quot;, &quot;/&quot; e &quot;/home&quot; exatamente da mesma forma que &quot;boot&quot;, &quot;root&quot; e &quot;home&quot;, respectivamente.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="706"/>
        <source>&lt;i&gt;Label&lt;/i&gt; - The label that is assigned to the partition once it has been formatted.</source>
        <translation>&lt;i&gt;Rótulo&lt;/i&gt; - O rótulo que será atribuído à partição, uma vez formatada. </translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="707"/>
        <source>&lt;i&gt;Encrypt&lt;/i&gt; - Use LUKS encryption for this partition. The password applies to all partitions selected for encryption.</source>
        <translation>&lt;i&gt;Encriptar&lt;/i&gt; -  Nesta partição usar a encriptação LUKS. A senha é a mesma para todas as partições seleccionadas para encriptação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="708"/>
        <source>&lt;i&gt;Format&lt;/i&gt; - This is the partition&apos;s format. Available formats depend on what the partition is used for. When working with an existing layout, you may be able to preserve the format of the partition by selecting &lt;b&gt;Preserve&lt;/b&gt;.</source>
        <translation>&lt;i&gt;Formato&lt;/i&gt; - Este é o formato da partição. Os formatos disponíveis dependem da finalidade da partição. Quando a trabalhar com uma organização existente, poderá ser possível preservar o formato da partição, seleccionando &lt;b&gt;Preservar&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="710"/>
        <source>Selecting &lt;b&gt;Preserve /home&lt;/b&gt; for the root partition preserves the contents of the /home directory, deleting everything else. This option can only be used when /home is on the same partition as the root.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../minstall.cpp" line="712"/>
        <source>The ext2, ext3, ext4, jfs, xfs and btrfs Linux filesystems are supported and ext4 is recommended.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../minstall.cpp" line="713"/>
        <source>&lt;i&gt;Check&lt;/i&gt; - Check and correct for bad blocks on the drive (not supported for all formats). This is very time consuming, so you may want to skip this step unless you suspect that your drive has bad blocks.</source>
        <translation>&lt;i&gt;Verificar&lt;/i&gt; - Verificar e corrigir os blocos defeituosos na unidade (não suportados para todos os formatos). Isto é muito demorado, pelo que poderá querer ignorar este passo, a menos que suspeite que a sua unidade tem blocos defeituosos.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="715"/>
        <source>&lt;i&gt;Mount Options&lt;/i&gt; - This specifies mounting options that will be used for this partition.</source>
        <translation>&lt;i&gt;Opções de montagem&lt;/i&gt; - Aqui são especificadas as opções de montagem para esta partição.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="716"/>
        <source>&lt;i&gt;Dump&lt;/i&gt; - Instructs the dump utility to include this partition in the backup.</source>
        <translation>&lt;i&gt;Despejar&lt;/i&gt; - Transmite ao utilitário &apos;dump&apos; (despejar) instruções para inclusão desta partição na cópia de segurança.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="717"/>
        <source>&lt;i&gt;Pass&lt;/i&gt; - The sequence in which this file system is to be checked at boot. If zero, the file system is not checked.</source>
        <translation>&lt;i&gt;Passar&lt;/i&gt; - A sequência pela qual este sistema de ficheiros será verificado no arranque pelo utilitário &apos;pass&apos;. Se zero, o sistema de ficheiros não será verificado.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="718"/>
        <source>Menus and actions</source>
        <translation>Menus e acções</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="719"/>
        <source>A variety of actions are available by right-clicking any drive or partition item in the list.</source>
        <translation>Um clique com o botão direito do rato sobre qualquer unidade ou partição da lista disponibiliza várias acções.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="720"/>
        <source>The buttons to the right of the list can also be used to manipulate the entries.</source>
        <translation>Os botões à direita da lista podem também ser usados para manipular as entradas.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="721"/>
        <source>The installer cannot modify the layout already on the drive. To create a custom layout, mark the drive for a new layout with the &lt;b&gt;New layout&lt;/b&gt; menu action or button (%1). This clears the existing layout.</source>
        <translation>O instalador não pode modificar a organização da unidade. Para criar uma organização própria, marcar a unidade a organizar com a acção de menu &lt;b&gt;Novo layout&lt;/b&gt; ou com o botão (%1). Isso apaga a organização existente.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="724"/>
        <source>Basic layout requirements</source>
        <translation>Requisitos básicos da organização</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="725"/>
        <source>%1 requires a root partition. The swap partition is optional but highly recommended. If you want to use the Suspend-to-Disk feature of %1, you will need a swap partition that is larger than your physical memory size.</source>
        <translation>O %1 requer uma partição raíz ( / , root). A partição de trocas (swap) é opcional mas altamente recomendada. Para usar a função Hibernar (Suspender-para-o-Disco) do %1 é necessária uma partição de trocas (swap) maior do que a memória RAM física no computador.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="727"/>
        <source>If you choose a separate /home partition it will be easier for you to upgrade in the future, but this will not be possible if you are upgrading from an installation that does not have a separate home partition.</source>
        <translation>Ter uma partição /home separada facilitará no futuro a substituição do sistema por uma nova versão. Mas se o sistema instalado não tiver numa partição /home separada, não será possível criá-la posteriormente.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="729"/>
        <source>Active partition</source>
        <translation>Partição ativa</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="730"/>
        <source>For the installed operating system to boot, the appropriate partition (usually the boot or root partition) must be the marked as active.</source>
        <translation>Para que o sistema operativo instalado arranque, a partição apropriada (geralmente a partição boot ou root) deve ser marcada como ativa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="731"/>
        <source>The active partition of a drive can be chosen using the &lt;b&gt;Active partition&lt;/b&gt; menu action.</source>
        <translation>A partição ativa de uma unidade pode ser escolhida utilizando a ação do menu &lt;b&gt;Partição ativa&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="732"/>
        <source>A partition with an asterisk (*) next to its device name is, or will become, the active partition.</source>
        <translation>Uma partição com um asterisco (*) ao lado do nome do dispositivo é, ou tornar-se-á, a partição ativa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="733"/>
        <source>Boot partition</source>
        <translation>Partição de arranque (boot)</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="734"/>
        <source>This partition is generally only required for root partitions on virtual devices such as encrypted, LVM or software RAID volumes.</source>
        <translation>Em regra esta partição só é requerida quando a partição raiz (root) está num dispositivo virtual,  isto é, um volume encriptado, LVM ou de software RAID.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="735"/>
        <source>It contains a basic kernel and drivers used to access the encrypted disk or virtual devices.</source>
        <translation>Contém um núcleo básico e controladores usados para aceder ao disco encriptado ou a dispositivos virtuais.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="736"/>
        <source>BIOS-GRUB partition</source>
        <translation>Partição BIOS-GRUB</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="737"/>
        <source>When using a GPT-formatted drive on a non-EFI system, a 1MB BIOS boot partition is required when using GRUB.</source>
        <translation>Se num sistema não-EFI for usada uma unidade formatada em GPT, para usar o GRUB é necessária uma partição de 1 MB de arranque (boot) por BIOS.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="738"/>
        <source>New drives are formatted in GPT if more than 4 partitions are to be created, or the drive has a capacity greater than 2TB. If the installer is about to format the disk in GPT, and there is no BIOS-GRUB partition, a warning will be displayed before the installation starts.</source>
        <translation>As novas unidades são formatadas em GPT se tiverem capacidade superior a 2 TB ou se forem criadas mais do que 4 partições. Se o instalador estiver prestes a formatar o disco em GPT e não existir uma partição BIOS-GRUB, antes de a instalação se iniciar será mostrado um aviso.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="740"/>
        <source>Need help creating a layout?</source>
        <translation>Ajuda para organizar o disco</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="741"/>
        <source>Just right-click on a drive to bring up a menu, and select a layout template. These layouts are similar to that of the regular install.</source>
        <translation>Clicar com o botão direito numa unidade para ver o menu e seleccionar o modelo de organização. Estas organizações são similares às de uma instalação normal.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="742"/>
        <source>&lt;i&gt;Standard install&lt;/i&gt; - Suited to most setups. This template does not add a separate boot partition, and so it is unsuitable for use with an encrypted operating system.</source>
        <translation>&lt;i&gt;Instalação padrão&lt;/i&gt; - Indicada para maioria dos casos. Este modelo não adiciona uma partição de arranque (boot) e por isso não pode ser usado com um sistema operativo encriptado.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="743"/>
        <source>&lt;i&gt;Encrypted system&lt;/i&gt; - Contains the boot partition required to load an encrypted operating system. This template can also be used as the basis for a multi-boot system.</source>
        <translation>&lt;i&gt;Sistema Encriptado&lt;/i&gt; - Contém a partição de arranque (boot) necessária para carregar um sistema operativo encriptado. Este modelo também pode ser usado como base para os casos de instalações múltiplas.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="744"/>
        <source>Upgrading</source>
        <translation>Actualizar</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="745"/>
        <source>To upgrade from an existing Linux installation, select the same home partition as before and select &lt;b&gt;Preserve&lt;/b&gt; as the format.</source>
        <translation>Para substituir um sistema (instalação) Linux por uma nova versão, seleccionar a mesma partição /home (caso exista) e escolher a opção &lt;b&gt;Preservar&lt;/b&gt;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="746"/>
        <source>If you do not use a separate home partition, select &lt;b&gt;Preserve /home&lt;/b&gt; on the root file system entry to preserve the existing /home directory located on your root partition. The installer will only preserve /home, and will delete everything else. As a result, the installation will take much longer than usual.</source>
        <translation>Se o sistema não tiver uma partição home, para preservar o diretório /home existente na partição raiz (root) seleccionar &lt;b&gt;Preservar /home&lt;/b&gt; na entrada do sistema de ficheiros raiz (root). O instalador preservará apenas o directório /home e eliminará tudo o resto. A instalação demorará muito mais do que o normal.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="748"/>
        <source>Preferred Filesystem Type</source>
        <translation>Tipo de Sistema de Ficheiros Preferido</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="749"/>
        <source>For %1, you may choose to format the partitions as ext2, ext3, ext4, f2fs, jfs, xfs or btrfs.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../minstall.cpp" line="750"/>
        <source>Additional compression options are available for drives using btrfs. Lzo is fast, but the compression is lower. Zlib is slower, with higher compression.</source>
        <translation>Partições formatadas como btrfs podem usar opções adicionais de compressão. O sistema lzo proporciona rapidez de compressão, mas a taxa de compressão é menor. O sistema zlib proporciona maior compressão, mas menor rapidez.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="752"/>
        <source>System partition management tool</source>
        <translation>Ferramenta para gestão de partições do sistema</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="753"/>
        <source>For more control over the drive layouts (such as modifying the existing layout on a disk), click the partition management button (%1). This will run the operating system&apos;s partition management tool, which will allow you to create the exact layout you need.</source>
        <translation>Para maior controlo sobre as organizações das unidades (como alterar a organização de um disco), clicar no botão Gestão de partições (%1). Isto irá executar a ferramenta de gestão de partições do sistema operativo, que irá permitir criar a organização pretendida.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="759"/>
        <source>To preserve an encrypted partition, right-click on it and select &lt;b&gt;Unlock&lt;/b&gt;. In the dialog that appears, enter a name for the virtual device and the password. When the device is unlocked, the name you chose will appear under &lt;i&gt;Virtual Devices&lt;/i&gt;, with similar options to that of a regular partition.</source>
        <translation>Para preservar uma partição encriptada, clicar sobre ela com o botão direito do rato e seleccionar &lt;b&gt;Desbloquear&lt;/b&gt;. Na caixa de diálogo que aparece, inserir um nome para o dispositivo virtual e a senha. Quando o dispositivo estiver desbloqueado, o nome escolhido aparecerá em &lt;i&gt;Dispositivos Virtuais&lt;/i&gt;, com opções similares às de uma partição normal.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="761"/>
        <source>For the encrypted partition to be unlocked at boot, it needs to be added to the crypttab file. Use the &lt;b&gt;Add to crypttab&lt;/b&gt; menu action to do this.</source>
        <translation>Para que a partição encriptada seja desbloqueada no arranque, tem de ser adicionada ao ficheiro crypttab. Para tal, usar a acção &lt;b&gt;Adicionar a crypttab&lt;/b&gt; do menu.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="762"/>
        <source>Other partitions</source>
        <translation>Outras partições</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="763"/>
        <source>The installer allows other partitions to be created or used for other purposes, however be mindful that older systems cannot handle drives with more than 4 partitions.</source>
        <translation>O instalador permite utilizar as partições para outros fins e criar novas partições. Mas é necessário ter atenção, pois sistemas mais antigos não conseguem lidar com discos com mais de 4 partições.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="764"/>
        <source>Subvolumes</source>
        <translation>Sub-volumes</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="765"/>
        <source>Some file systems, such as Btrfs, support multiple subvolumes in a single partition. These are not physical subdivisions, and so their order does not matter.</source>
        <translation>Alguns sistemas de ficheiros, como o Btrfs, suportam vários sub-volumes numa única partição. Os sub-volumes não são sub-divisões físicas, pelo que a sua ordem é irrelevante.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="767"/>
        <source>Use the &lt;b&gt;Scan subvolumes&lt;/b&gt; menu action to search an existing Btrfs partition for subvolumes. To create a new subvolume, use the &lt;b&gt;New subvolume&lt;/b&gt; menu action.</source>
        <translation>Usar a acção &lt;b&gt;Procurar sub-volumes&lt;/b&gt; do menu para procurar sub-volumes numa partição Btrfs. Para criar um novo sub-volume, usar a ação &lt;b&gt;Novo sub-volume&lt;/b&gt;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="769"/>
        <source>Existing subvolumes can be preserved, however the name must remain the same.</source>
        <translation>Os sub-volumes existentes podem ser preservados, sendo que o nome tem de se manter.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="770"/>
        <source>Virtual Devices</source>
        <translation>Dispositivos Virtuais</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="771"/>
        <source>If the intaller detects any virtual devices such as opened LUKS partitions, LVM logical volumes or software-based RAID volumes, they may be used for the installation.</source>
        <translation>Se o instalador detectar quaisquer dispositivos virtuais, tais como partições LUKS abertas, volumes lógicos LVM ou volumes RAID baseados em software, esses volumes podem ser usados para a instalação.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="772"/>
        <source>The use of virtual devices (beyond preserving encrypted file systems) is an advanced feature. You may have to edit some files (eg. initramfs, crypttab, fstab) to ensure the virtual devices used are created upon boot.</source>
        <translation>O uso de dispositivos virtuais (para além da função de preservação de sistemas de ficheiros encriptados) é um recurso avançado. Podem ter que ser editados alguns ficheiros (initramfs, crypttab e fstab, por exemplo) para garantir que os dispositivos virtuais a usar sejam criados no arranque.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="777"/>
        <source>&lt;p&gt;&lt;b&gt;Select Boot Method&lt;/b&gt;&lt;br/&gt; %1 uses the GRUB bootloader to boot %1 and MS-Windows. &lt;p&gt;By default GRUB2 is installed in the Master Boot Record (MBR) or ESP (EFI System Partition for 64-bit UEFI boot systems) of your boot drive and replaces the boot loader you were using before. This is normal.&lt;/p&gt;&lt;p&gt;If you choose to install GRUB2 to Partition Boot Record (PBR) instead, then GRUB2 will be installed at the beginning of the specified partition. This option is for experts only.&lt;/p&gt;&lt;p&gt;If you uncheck the Install GRUB box, GRUB will not be installed at this time. This option is for experts only.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Opções de Arranque&lt;/b&gt;&lt;br/&gt;O %1 usa o carregador do arranque GRUB2 para carregar o arranque do sistema a usar (%1, MS-Windows ou outro instalado). &lt;p&gt;Por predefinição o GRUB2 será instalado no MBR [Master Boot Record] ou na ESP [EFI System Partition] (no caso de sistemas de 64-bit com UEFI) do disco (do disco de arranque se o computador tiver mais do que um disco) e substituirá o carregador do arranque aí instalado. Isto é normal.&lt;/p&gt;&lt;p&gt;Optando por instalar o GRUB2 na raíz do sistema, ele será instalado no início da partição escolhida. Utilizadores inexperientes não devem usar esta opção.&lt;/p&gt;&lt;p&gt;Desmarcando a caixa &apos;Instalar GRUB&apos;, o GRUB não será instalado. Utilizadores inexperientes não devem usar esta opção.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="790"/>
        <source>&lt;p&gt;&lt;b&gt;Common Services to Enable&lt;/b&gt;&lt;br/&gt;Select any of these common services that you might need with your system configuration and the services will be started automatically when you start %1.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Activar Serviços de Uso Frequente&lt;/b&gt;&lt;br/&gt;Seleccionar quaisquer destes serviços e eles serão iniciados automaticamente ao iniciar o %1.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="794"/>
        <source>&lt;p&gt;&lt;b&gt;Computer Identity&lt;/b&gt;&lt;br/&gt;The computer name is a common unique name which will identify your computer if it is on a network. The computer domain is unlikely to be used unless your ISP or local network requires it.&lt;/p&gt;&lt;p&gt;The computer and domain names can contain only alphanumeric characters, dots, hyphens. They cannot contain blank spaces, start or end with hyphens&lt;/p&gt;&lt;p&gt;The SaMBa Server needs to be activated if you want to use it to share some of your directories or printer with a local computer that is running MS-Windows or Mac OSX.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Identificação do Computador&lt;/b&gt;&lt;br/&gt;O nome do computador é um nome específico que permitirá identificar o computador numa rede a que esteja ligado. Não é provável que o domínio do computador seja usado, a menos que o fornecedor de Internet ou a rede local o requeiram.&lt;/p&gt;&lt;p&gt;Os nomes de computador e de domínio apenas podem conter caracteres alfanuméricos, pontos e hífens. Não podem conter espaços em branco, nem começar ou terminar com hífens.&lt;/p&gt;&lt;p&gt;Para partilhar pastas ou uma impressora com um computador local que opere com MS-Windows ou Mac OSX deve ser activado o servidor SaMBa (aplicação servidora de ficheiros).&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="804"/>
        <source>Localization Defaults</source>
        <translation>Predefinições de localização</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="805"/>
        <source>Set the default locale. This will apply unless they are overridden later by the user.</source>
        <translation>Predefinir o idioma e a zona horária. Serão sempre usados, excepto se alterados posteriormente pelo utilizador.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="806"/>
        <source>Configure Clock</source>
        <translation>Configurar o relógio</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="807"/>
        <source>If you have an Apple or a pure Unix computer, by default the system clock is set to Greenwich Meridian Time (GMT) or Coordinated Universal Time (UTC). To change this, check the &quot;&lt;b&gt;System clock uses local time&lt;/b&gt;&quot; box.</source>
        <translation>Em computadores Apple ou puro Unix, por predefinição o relógio do sistema assume a Hora Média de Greenwich (GMT) ou Tempo Universal Coordenado (UTC). Para alterar, seleccionar a caixa &quot;&lt;b&gt; O relógio do sistema usa o tempo local&lt;/b&gt;&quot;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="809"/>
        <source>The system boots with the timezone preset to GMT/UTC. To change the timezone, after you reboot into the new installation, right click on the clock in the Panel and select Properties.</source>
        <translation>O sistema arranca com a zona horário pré definida para GMT/UTC. Para alterar a zona horária, após arrancar de novo após terminada a nova instalação, clicar com o botão direito do rato no relógio do Painel e escolher Propriedades.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="811"/>
        <source>Service Settings</source>
        <translation>Definições de Serviços</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="812"/>
        <source>Most users should not change the defaults. Users with low-resource computers sometimes want to disable unneeded services in order to keep the RAM usage as low as possible. Make sure you know what you are doing!</source>
        <translation>A maioria dos utilizadores não deverá alterar as predefinições. Utilizadores de computadores com poucos recursos, por vezes querem desactivar serviços desnecessários, para manter o uso da RAM o mais baixo possível. Não fazer nada sem a certeza do que for feito!</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="818"/>
        <source>Default User Login</source>
        <translation>Acesso do Utilizador Predefinido</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="819"/>
        <source>The root user is similar to the Administrator user in some other operating systems. You should not use the root user as your daily user account. Please enter the name for a new (default) user account that you will use on a daily basis. If needed, you can add other user accounts later with %1 User Manager.</source>
        <translation>O utilizador root é similar ao Administrador de outros sistemas operativos. Na utilização comum do dia-a-dia não deve ser usada a conta de utilizador root. Inserir o nome para um novo utilizador, que ficará predefinido como utilizador normal no dia-a-dia. Se necessário, posteriormente poderão ser adicionados outros utilizadores, usando o Gestor de Utilizadores do %1.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="823"/>
        <source>Passwords</source>
        <translation>Senhas </translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="824"/>
        <source>Enter a new password for your default user account and for the root account. Each password must be entered twice.</source>
        <translation>Inserir senhas para as contas do utilizador predefinido e do utilizador root (administrador). Cada uma das senhas deverá ser inserida duas vezes.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="826"/>
        <source>No passwords</source>
        <translation>Sem senhas</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="827"/>
        <source>If you want the default user account to have no password, leave its password fields empty. This allows you to log in without requiring a password.</source>
        <translation>Para que a conta do utilizador predefinido não tenha senha, deixar os campos de senha vazios. Isso irá permitir aceder ao sistema sem senha. </translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="829"/>
        <source>Obviously, this should only be done in situations where the user account does not need to be secure, such as a public terminal.</source>
        <translation>Obviamente, isto só deverá ser feito em situações onde a conta do utilizador não necessita de ser segura, o que não é o caso dos terminais públicos, p. ex.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="837"/>
        <source>Old Home Directory</source>
        <translation>Diretório Home Existente</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="838"/>
        <source>A home directory already exists for the user name you have chosen. This screen allows you to choose what happens to this directory.</source>
        <translation>Já existe uma pasta de utilizador para o nome de utilizador escolhido. Este quadro permite escolher o que fazer desta pasta.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="840"/>
        <source>Re-use it for this installation</source>
        <translation>Re-usar para esta instalação</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="841"/>
        <source>The old home directory will be used for this user account. This is a good choice when upgrading, and your files and settings will be readily available.</source>
        <translation>A pasta de utilizador existente será usada para esta conta de utilizador. Esta é uma boa opção para o caso de actualização do sistema com uma versão mais recente, pois os ficheiros e definições do utilizador ficarão imediatamente disponíveis.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="843"/>
        <source>Rename it and create a new directory</source>
        <translation>Renomear e criar uma nova pasta</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="844"/>
        <source>A new home directory will be created for the user, but the old home directory will be renamed. Your files and settings will not be immediately visible in the new installation, but can be accessed using the renamed directory.</source>
        <translation>Será criada uma nova pasta de utilizador e a pasta existente será renomeada. Os ficheiros e definições do utilizador não ficarão imediatamente visíveis na nova instalação, mas podem ser acedidos usando a pasta renomeada.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="846"/>
        <source>The old directory will have a number at the end of it, depending on how many times the directory has been renamed before.</source>
        <translation>A pasta de utilizador existente ficará com um número no fim, que dependerá das vezes que a pasta tenha sido renomeada antes.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="847"/>
        <source>Delete it and create a new directory</source>
        <translation>Eliminar e criar uma nova pasta de utilizador</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="848"/>
        <source>The old home directory will be deleted, and a new one will be created from scratch.</source>
        <translation>A pasta de utilizador existente será eliminada e será criada uma nova.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="849"/>
        <source>Warning</source>
        <translation>Aviso</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="850"/>
        <source>All files and settings will be deleted permanently if this option is selected. Your chances of recovering them are low.</source>
        <translation>Se esta opção for seleccionada, todos os ficheiros e definições serão permanentemente eliminados. A probabilidade de virem a ser recuparados é baixa.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="866"/>
        <source>Installation in Progress</source>
        <translation>Instalação em curso</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="867"/>
        <source>%1 is installing. For a fresh install, this will probably take 3-20 minutes, depending on the speed of your system and the size of any partitions you are reformatting.</source>
        <translation>O %1 está a ser instalado. Uma instalação nova irá demorar de 3 a 20 minutos, provavelmente, dependendo da velocidade do sistema e do tamanho das partições a reformatar.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="869"/>
        <source>If you click the Abort button, the installation will be stopped as soon as possible.</source>
        <translation>Clicando no botão Abortar, a instalação será interrompida assim que possível.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="871"/>
        <source>Change settings while you wait</source>
        <translation>Alterar definições enquanto à espera</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="872"/>
        <source>While %1 is being installed, you can click on the &lt;b&gt;Next&lt;/b&gt; or &lt;b&gt;Back&lt;/b&gt; buttons to enter other information required for the installation.</source>
        <translation>Enquanto o %1 está a ser instalado, é possível introduzir outra informação requerida clicando nos botões &lt;b&gt;Seguinte&lt;/b&gt; e &lt;b&gt;Anterior&lt;/b&gt;.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="874"/>
        <source>Complete these steps at your own pace. The installer will wait for your input if necessary.</source>
        <translation>Completar estes passos sem pressa. O instalador aguardará pela introdução de informação, se necessário.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="881"/>
        <source>&lt;p&gt;&lt;b&gt;Congratulations!&lt;/b&gt;&lt;br/&gt;You have completed the installation of %1&lt;/p&gt;&lt;p&gt;&lt;b&gt;Finding Applications&lt;/b&gt;&lt;br/&gt;There are hundreds of excellent applications installed with %1 The best way to learn about them is to browse through the Menu and try them. Many of the apps were developed specifically for the %1 project. These are shown in the main menus. &lt;p&gt;In addition %1 includes many standard Linux applications that are run only from the command line and therefore do not show up in the Menu.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Parabéns!&lt;/b&gt;&lt;br/&gt;A instalação do %1 está concluída. &lt;/p&gt;&lt;p&gt;&lt;b&gt;Encontrar Aplicações&lt;/b&gt;&lt;br/&gt;Com o %1 são instaladas centenas de excelentes aplicações. A melhor maneira de se familiarizar com elas é explorar o Menu e experimentá-las. Muitas delas foram desenvolvidas especificamente para o projecto %1. Essas são acedidas pelos menus principais. &lt;p&gt;O %1 inclui também muitas aplicações linux que só são executáveis a partir da linha de comando, pelo que não aparecem no Menu.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="890"/>
        <source>Enjoy using %1&lt;/b&gt;&lt;/p&gt;</source>
        <translation>Usufrua do %1&lt;/b&gt;&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="891"/>
        <location filename="../minstall.cpp" line="1194"/>
        <source>&lt;p&gt;&lt;b&gt;Support %1&lt;/b&gt;&lt;br/&gt;%1 is supported by people like you. Some help others at the support forum - %2 - or translate help files into different languages, or make suggestions, write documentation, or help test new software.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Apoiar o %1&lt;/b&gt;&lt;br/&gt;O %1 é apoiado por pessoas como você. Há pessoas que ajudam outros nos fóruns de apoio - %2 - ou traduzem ficheiros da ajuda para outros idiomas, fazem sugestões, elaboram documentação ou ajudam a testar novo software.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="921"/>
        <source>Finish</source>
        <translation>Terminar</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="924"/>
        <source>OK</source>
        <translation>Aceitar</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="926"/>
        <source>Next</source>
        <translation>Seguinte</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="964"/>
        <source>Configuring sytem. Please wait.</source>
        <translation>A configurar sistema. Aguardar.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="968"/>
        <source>Configuration complete. Restarting system.</source>
        <translation>Configuração completa. A reiniciar.</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="991"/>
        <location filename="../minstall.cpp" line="1307"/>
        <source>Root</source>
        <translation>Root</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="993"/>
        <location filename="../minstall.cpp" line="1315"/>
        <source>Home</source>
        <translation>Home</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1128"/>
        <source>Confirmation</source>
        <translation>Confirmação</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1128"/>
        <source>The installation and configuration is incomplete.
Do you really want to stop now?</source>
        <translation>A instalação e configuração está incompletas.
Interromper realmente o processo agora?</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1180"/>
        <source>&lt;p&gt;&lt;b&gt;Getting Help&lt;/b&gt;&lt;br/&gt;Basic information about %1 is at %2.&lt;/p&gt;&lt;p&gt;There are volunteers to help you at the %3 forum, %4&lt;/p&gt;&lt;p&gt;If you ask for help, please remember to describe your problem and your computer in some detail. Usually statements like &apos;it didn&apos;t work&apos; are not helpful.&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Obter Ajuda&lt;/b&gt;&lt;br/&gt;Informação básica sobre o %1 em %2.&lt;/p&gt;&lt;p&gt;Há voluntários que prestam ajuda no fórum do %3, %4 (maioritariamente em inglês)&lt;/p&gt;&lt;p&gt;Ao solicitar ajuda, ter presente a necessidade de pormenorizar suficientemente a descrição do problema, bem como a informação sobre o computador. Por regra, afirmações como &apos;(algo) não funcionou&apos; não ajudam a proporcionar boa ajuda.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1188"/>
        <source>&lt;p&gt;&lt;b&gt;Repairing Your Installation&lt;/b&gt;&lt;br/&gt;If %1 stops working from the hard drive, sometimes it&apos;s possible to fix the problem by booting from LiveDVD or LiveUSB and running one of the included utilities in %1 or by using one of the regular Linux tools to repair the system.&lt;/p&gt;&lt;p&gt;You can also use your %1 LiveDVD or LiveUSB to recover data from MS-Windows systems!&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Reparar o Sistema (Instalação)&lt;/b&gt;&lt;br/&gt;Se o %1 deixar de funcionar, por vezes é possível reparar o problema através do DVD-executável ou da USB-executável, usando algum dos utilitários do %1 ou uma das ferramentas padrão do Linux.&lt;/p&gt;&lt;p&gt;O %1 em DVD ou penUSB executáveis pode também ser usado para recuperar dados de sistemas MS-Windows!&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1202"/>
        <source>&lt;p&gt;&lt;b&gt;Adjusting Your Sound Mixer&lt;/b&gt;&lt;br/&gt; %1 attempts to configure the sound mixer for you but sometimes it will be necessary for you to turn up volumes and unmute channels in the mixer in order to hear sound.&lt;/p&gt; &lt;p&gt;The mixer shortcut is located in the menu. Click on it to open the mixer. &lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Ajustar o Misturador de Som&lt;/b&gt;&lt;br/&gt;O %1 procura configurar o misturador de som, mas por vezes é necessário aumentar o volume ou desativar a função &apos;mute&apos; nos canais do misturador para que se possa ouvir o som.&lt;/p&gt; &lt;p&gt;O atalho para o misturador está localizado no menu. Clicar para abrir o misturador.&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1210"/>
        <source>&lt;p&gt;&lt;b&gt;Keep Your Copy of %1 up-to-date&lt;/b&gt;&lt;br/&gt;For more information and updates please visit&lt;/p&gt;&lt;p&gt; %2&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Manter o %1 actualizado&lt;/b&gt;&lt;br/&gt;Para mais informação e actualização do %1, visitar&lt;/p&gt;&lt;p&gt; %2&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1215"/>
        <source>&lt;p&gt;&lt;b&gt;Special Thanks&lt;/b&gt;&lt;br/&gt;Thanks to everyone who has chosen to support %1 with their time, money, suggestions, work, praise, ideas, promotion, and/or encouragement.&lt;/p&gt;&lt;p&gt;Without you there would be no %1.&lt;/p&gt;&lt;p&gt;%2 Dev Team&lt;/p&gt;</source>
        <translation>&lt;p&gt;&lt;b&gt;Agradecimentos&lt;/b&gt;&lt;br/&gt;Agradecemos a todos os que decidiram apoiar o %1 com o seu tempo, os seus donativos, as suas sugestões, o seu trabalho, os seus elogios, as suas ideias, promovendo-o e/ou encorajando-nos.&lt;/p&gt;&lt;p&gt;Sem eles o %1 não existiria.&lt;/p&gt;&lt;p&gt;A equipa de desenvolvimento do %2&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1266"/>
        <source>%1% root
%2% home</source>
        <translation>%1% root
%2% home</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1268"/>
        <source>Combined root and home</source>
        <translation>Root e home combinadas</translation>
    </message>
    <message>
        <location filename="../minstall.cpp" line="1312"/>
        <source>----</source>
        <translation>----</translation>
    </message>
</context>
<context>
    <name>MPassEdit</name>
    <message>
        <location filename="../mpassedit.cpp" line="101"/>
        <source>Use password</source>
        <translation>Usar senha</translation>
    </message>
    <message>
        <location filename="../mpassedit.cpp" line="188"/>
        <source>Hide the password</source>
        <translation>Ocultar a senha</translation>
    </message>
    <message>
        <location filename="../mpassedit.cpp" line="188"/>
        <source>Show the password</source>
        <translation>Mostrar a senha</translation>
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
        <translation>Registo (Log)  </translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="107"/>
        <source>Close</source>
        <translation>Fechar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="118"/>
        <source>Next</source>
        <translation>Seguinte</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="125"/>
        <source>Alt+N</source>
        <translation>Alt+N</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="166"/>
        <source>Gathering Information, please stand by.</source>
        <translation>A recolher informação. Aguardar.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="201"/>
        <source>Terms of Use</source>
        <translation>Termos de Utilização</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="228"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;center&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Keyboard Settings&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;center&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Definições do Teclado&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
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
        <translation>Alterar as definições do teclado</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="266"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;right&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Layout:&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p align=&quot;right&quot;&gt;&lt;span style=&quot; font-weight:600;&quot;&gt;Esquema:&lt;/span&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="317"/>
        <source>Select type of installation</source>
        <translation>Seleccionar o tipo de instalação</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="329"/>
        <source>Regular install using the entire disk</source>
        <translation>Instalação normal, a utilizar todo o disco.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="342"/>
        <source>Customize the disk layout</source>
        <translation>Organização personalizada do disco</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="355"/>
        <source>Use disk:</source>
        <translation>Usar o disco:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="368"/>
        <source>Encrypt</source>
        <translation>Encriptar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="380"/>
        <location filename="../meinstall.ui" line="669"/>
        <source>Encryption password:</source>
        <translation>Senha de encriptação</translation>
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
        <translation>Inquirir o sistema operativo e recarregar os esquemas de todas as unidades.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="587"/>
        <source>Remove an existing entry from the layout. This only works with entries to a new layout.</source>
        <translation>Remover uma entrada do esquema. Apenas funciona com entradas numa nova organização.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="598"/>
        <source>Add a new partition entry. This only works with a new layout.</source>
        <translation>Adicionar uma nova partição. Só funciona numa nova organização.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="609"/>
        <source>Mark the selected drive to be cleared for a new layout.</source>
        <translation>Marcar a unidade seleccionada para ser apagada para uma nova organização.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="630"/>
        <source>Run the partition management application of this operating system.</source>
        <translation>Executar a aplicação de gestão de partições deste sistema operativo.</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="653"/>
        <source>Encryption options</source>
        <translation>Opções de encriptação</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="728"/>
        <source>Install GRUB for Linux and Windows</source>
        <translation>Instalar o GRUB para Linux e Windows</translation>
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
        <translation>Disco de arranque do sistema:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="841"/>
        <source>Location to install on:</source>
        <translation>Local onde instalar:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="893"/>
        <source>Common Services to Enable</source>
        <translation>Serviços Comuns a Activar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="912"/>
        <source>Service</source>
        <translation>Serviço</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="917"/>
        <source>Description</source>
        <translation>Descrição</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="950"/>
        <source>Computer Network Names</source>
        <translation>Nomes de redes de computadores</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="977"/>
        <source>Workgroup</source>
        <translation>Grupo de trabalho</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="990"/>
        <source>Workgroup:</source>
        <translation>Grupo de trabalho:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1003"/>
        <source>SaMBa Server for MS Networking</source>
        <translation>Servidor de ficheiros SaMBa para interacção com sistemas MS Windows</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1019"/>
        <source>example.dom</source>
        <translation>exemplo.dom</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1032"/>
        <source>Computer domain:</source>
        <translation>Domínio do computador:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1058"/>
        <source>Computer name:</source>
        <translation>Nome do computador:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1113"/>
        <source>Configure Clock</source>
        <translation>Configurar o relógio</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1148"/>
        <source>Format:</source>
        <translation>Formato:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1176"/>
        <source>Timezone:</source>
        <translation>Zona Horária:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1215"/>
        <source>System clock uses local time</source>
        <translation>O relógio do sistema usa a hora local</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1241"/>
        <source>Localization Defaults</source>
        <translation>Predefinições de localização</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1281"/>
        <source>Locale:</source>
        <translation>Localização:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1303"/>
        <source>Service Settings (advanced)</source>
        <translation>Definições de serviços (avançadas)</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1321"/>
        <source>Adjust which services should run at startup</source>
        <translation>Definir os serviços a serem activados ao iniciar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1324"/>
        <source>View</source>
        <translation>Ver</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1363"/>
        <source>Desktop modifications made in the live environment will be carried over to the installed OS</source>
        <translation>As alterações ao ambiente de trabalho feitas na sessão da instalação externa serão transpostas para o sistema instalado no disco rígido</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1366"/>
        <source>Save live desktop changes</source>
        <translation>Guardar as alterações feitas na sessão da instalação externa</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1379"/>
        <source>Default User Account</source>
        <translation>Conta do utilizador predefinido</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1391"/>
        <source>Default user login name:</source>
        <translation>Nome de acesso do utilizador predefinido:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1407"/>
        <source>Default user password:</source>
        <translation>Senha do utilizador predefinido:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1420"/>
        <source>Confirm user password:</source>
        <translation>Confirmar a senha de utilizador:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1481"/>
        <source>username</source>
        <translation>utilizador</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1497"/>
        <source>Root (administrator) Account</source>
        <translation>Conta root (administrador)</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1515"/>
        <source>Root password:</source>
        <translation>Senha de root:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1560"/>
        <source>Confirm root password:</source>
        <translation>Confirmar a senha de root:</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1589"/>
        <source>Autologin</source>
        <translation>Acesso automático</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1619"/>
        <source>Existing Home Directory</source>
        <translation>Pasta de Utilizador existente</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1628"/>
        <source>What would you like to do with the old directory?</source>
        <translation>O que fazer com a pasta existente?</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1635"/>
        <source>Re-use it for this installation</source>
        <translation>Re-usar para esta instalação</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1642"/>
        <source>Rename it and create a new directory</source>
        <translation>Renomear e criar uma nova pasta de utilizador</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1649"/>
        <source>Delete it and create a new directory</source>
        <translation>Eliminar e criar uma nova pasta de utilizador</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1688"/>
        <source>Tips</source>
        <translation>Dicas</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1732"/>
        <source>Installation complete</source>
        <translation>Instalação concluída</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1738"/>
        <source>Automatically reboot the system when the installer is closed</source>
        <translation>Reiniciar automaticamente o sistema quando o instalador se fechar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1757"/>
        <source>Reminders</source>
        <translation>Lembretes</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1792"/>
        <source>Back</source>
        <translation>Anterior</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1799"/>
        <source>Alt+K</source>
        <translation>Alt+K</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1812"/>
        <source>Installation in progress</source>
        <translation>Instalação em curso</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1827"/>
        <source>Abort</source>
        <translation>Abortar</translation>
    </message>
    <message>
        <location filename="../meinstall.ui" line="1830"/>
        <source>Alt+A</source>
        <translation>Alt+A</translation>
    </message>
</context>
<context>
    <name>Oobe</name>
    <message>
        <location filename="../oobe.cpp" line="320"/>
        <source>Please enter a computer name.</source>
        <translation>Introduzir o nome do computador.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="324"/>
        <source>Sorry, your computer name contains invalid characters.
You'll have to select a different
name before proceeding.</source>
        <translation>O nome do computador contém caracteres inválidos.
Tem que ser escolhido um nome
diferente antes de prosseguir.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="329"/>
        <source>Please enter a domain name.</source>
        <translation>Introduzir um nome de domínio.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="333"/>
        <source>Sorry, your computer domain contains invalid characters.
You'll have to select a different
name before proceeding.</source>
        <translation>O nome de domínio do computador contém caracteres
inválidos. Tem que ser escolhido
um nome diferente antes de prosseguir.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="340"/>
        <source>Please enter a workgroup.</source>
        <translation>Introduzir um grupo de trabalho.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="475"/>
        <source>The user name cannot contain special characters or spaces.
Please choose another name before proceeding.</source>
        <translation>O nome de utilizador não pode conter caracteres especiais ou espaços.
Escolher outro nome antes de prosseguir.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="486"/>
        <source>Sorry, that name is in use.
Please select a different name.</source>
        <translation>Este nome já está em uso.
Escolher um nome diferente.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="495"/>
        <source>You did not provide a password for the root account. Do you want to continue?</source>
        <translation>Não foi introduzida nenhuma senha para a conta root. Continuar?</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="524"/>
        <source>Failed to set user account passwords.</source>
        <translation>Falha ao criar senhas para a conta de utilizador.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="543"/>
        <source>Failed to save old home directory.</source>
        <translation>Falha ao guardar a pasta pessoal existente.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="547"/>
        <source>Failed to delete old home directory.</source>
        <translation>Falha ao apagar a pasta pessoal existente.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="568"/>
        <source>Sorry, failed to create user directory.</source>
        <translation>A criação da pasta de utilizador falhou.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="572"/>
        <source>Sorry, failed to name user directory.</source>
        <translation>A atribuição de nome à pasta do utilizador falhou.</translation>
    </message>
    <message>
        <location filename="../oobe.cpp" line="607"/>
        <source>Sorry, failed to set ownership of user directory.</source>
        <translation>A atribuição de detenção (ownership) à pasta do utilizador falhou.</translation>
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
        <translation>&amp;Adicionar partição</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="452"/>
        <source>&amp;Remove partition</source>
        <translation>&amp;Remover partição</translation>
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
        <translation>Adicionar a crypttab</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="477"/>
        <source>Active partition</source>
        <translation>Partição ativa</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="483"/>
        <source>New subvolume</source>
        <translation>Novo sub-volume</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="484"/>
        <source>Scan subvolumes</source>
        <translation>Procurar sub-volumes</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="507"/>
        <source>New &amp;layout</source>
        <translation>Nova &amp;organização</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="508"/>
        <source>&amp;Reset layout</source>
        <translation>&amp;Restabelecer organização</translation>
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
        <translation>&amp;Sistema encriptado</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="530"/>
        <source>Remove subvolume</source>
        <translation>Remover sub-volume</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="598"/>
        <source>Unlock Drive</source>
        <translation>Desbloquear Unidade</translation>
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
        <translation>Não foi possível desbloquear o dispositivo. Senha incorreta, possivelmente.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="663"/>
        <source>Failed to close %1</source>
        <translation>Falha ao fechar %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="707"/>
        <source>Invalid subvolume label</source>
        <translation>Rótulo de sub-volume inválido</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="718"/>
        <source>Duplicate subvolume label</source>
        <translation>Rótulo de sub-volume duplicado</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="727"/>
        <source>Invalid use for %1: %2</source>
        <translation>Uso inválido de %1: %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="738"/>
        <source>%1 is already selected for: %2</source>
        <translation>%1 já está selecionado para: %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="763"/>
        <source>A root partition of at least %1 is required.</source>
        <translation>É necessária uma partição root de pelo menos %1.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="767"/>
        <source>Cannot preserve /home inside root (/) if a separate /home partition is also mounted.</source>
        <translation>É impossível preservar /home em root (/) se uma partição /home separada estiver montada.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="776"/>
        <source>You must choose a separate boot partition when encrypting root.</source>
        <translation>Tem que ser escolhida uma partição de arranque (boot) diferente quando a raiz (root) for encriptada.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="788"/>
        <source>Prepare %1 partition table on %2</source>
        <translation>Preparar a tabela de partições %1 em %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="797"/>
        <source>Format %1 to use for %2</source>
        <translation>Formatar %1 a utilizar para %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="798"/>
        <source>Reuse (no reformat) %1 as %2</source>
        <translation>Reutilizar (sem reformatar) %1 como %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="799"/>
        <source>Delete the data on %1 except for /home, to use for %2</source>
        <translation>Apagar os dados em %1, excepto os dados de /home a usar em %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="801"/>
        <source>Create %1 without formatting</source>
        <translation>Criar %1 sem formatação</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="802"/>
        <source>Create %1, format to use for %2</source>
        <translation>Criar %1, formato a utilizar para %2</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="819"/>
        <source>The following drives are, or will be, setup with GPT, but do not have a BIOS-GRUB partition:</source>
        <translation>As unidades seguintes estão, ou serão, formatadas em GPT, mas não têm uma partição BIOS-GRUB:</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="821"/>
        <source>This system may not boot from GPT drives without a BIOS-GRUB partition.</source>
        <translation>Este sistema não pode arrancar a partir de unidades formatadas em GPT que não tenham uma partição BIOS-GRUB.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="822"/>
        <source>Are you sure you want to continue?</source>
        <translation>Continuar?</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="826"/>
        <source>The %1 installer will now perform the requested actions.</source>
        <translation>O instalador do %1 irá agora executar as acções solicitadas.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="827"/>
        <source>These actions cannot be undone. Do you want to continue?</source>
        <translation>Estas acções não podem ser revertidas. Continuar?</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="866"/>
        <source>The disks with the partitions you selected for installation are failing:</source>
        <translation>Os discos com as partições seleccionadas para a instalação estão a falhar:</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="870"/>
        <source>Smartmon tool output:</source>
        <translation>Resultados da ferramenta Smartmon:</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="871"/>
        <source>The disks with the partitions you selected for installation pass the SMART monitor test (smartctl), but the tests indicate it will have a higher than average failure rate in the near future.</source>
        <translation>Os discos com as partições seleccionadas para instalação passaram no teste de monitorização SMART (smartctl), mas os testes indicam que terão  uma taxa de falha superior à média no futuro próximo.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="876"/>
        <source>If unsure, please exit the Installer and run GSmartControl for more information.</source>
        <translation>Na dúvida, sair do Instalador e executar o GSmartControl para mais informação.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="878"/>
        <source>Do you want to abort the installation?</source>
        <translation>Abortar a instalação? </translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="883"/>
        <source>Do you want to continue?</source>
        <translation>Continuar?</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="898"/>
        <source>Failed to format LUKS container.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../partman.cpp" line="907"/>
        <source>Failed to open LUKS container.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../partman.cpp" line="957"/>
        <source>Failed to prepare required partitions.</source>
        <translation>Falha ao preparar as partições requeridas.</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="988"/>
        <source>Preparing partition tables</source>
        <translation>A preparar as tabelas de partições</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1004"/>
        <source>Preparing required partitions</source>
        <translation>A preparar as partições necessárias</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1070"/>
        <source>Creating encrypted volume: %1</source>
        <translation>A criar volume encriptado: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1075"/>
        <source>Formatting: %1</source>
        <translation>A formatar: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1078"/>
        <source>Failed to format partition.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../partman.cpp" line="1136"/>
        <source>Failed to prepare subvolumes.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../partman.cpp" line="1145"/>
        <source>Preparing subvolumes</source>
        <translation>A preparar os sub-volumes</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1319"/>
        <source>Failed to mount partition.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../partman.cpp" line="1322"/>
        <source>Mounting: %1</source>
        <translation>A montar: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1482"/>
        <source>Model: %1</source>
        <translation>Modelo: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1490"/>
        <source>Free space: %1</source>
        <translation>Espaço livre: %1</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1575"/>
        <source>Device</source>
        <translation>Dispositivo</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1576"/>
        <source>Size</source>
        <translation>Tamanho</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1577"/>
        <source>Use For</source>
        <translation>Usar Para</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1578"/>
        <source>Label</source>
        <translation>Rótulo</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1579"/>
        <source>Encrypt</source>
        <translation>Encriptar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1580"/>
        <source>Format</source>
        <translation>Formato</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1581"/>
        <source>Check</source>
        <translation>Verificar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1582"/>
        <source>Options</source>
        <translation>Opções</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1583"/>
        <source>Dump</source>
        <translation>Despejar</translation>
    </message>
    <message>
        <location filename="../partman.cpp" line="1584"/>
        <source>Pass</source>
        <translation>Passar</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../app.cpp" line="63"/>
        <source>Customizable GUI installer for MX Linux and antiX Linux</source>
        <translation>Instalador para MX Linux e antiX Linux com Interface gráfica, personalizável </translation>
    </message>
    <message>
        <location filename="../app.cpp" line="66"/>
        <source>Installs automatically using the configuration file (more information below).
-- WARNING: potentially dangerous option, it will wipe the partition(s) automatically.</source>
        <translation>Instalar usando automaticamente o ficheiro de configuração (mais informação em baixo).
--AVISO: opção potencialmente perigosa, irá apagar as partições automaticamente. </translation>
    </message>
    <message>
        <location filename="../app.cpp" line="68"/>
        <source>Overrules sanity checks on partitions and drives, causing them to be displayed.
-- WARNING: this can break things, use it only if you don&apos;t care about data on drive.</source>
        <translation>Ignora as verificações de sanidade das partições e unidades, fazendo com que sejam exibidas.
--AVISO: isto pode danificar algumas coisas; usar apenas se os dados que estiverem na unidade forem para descartar.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="70"/>
        <source>Load a configuration file as specified by &lt;config-file&gt;.
By default /etc/minstall.conf is used.
This configuration can be used with --auto for an unattended installation.
The installer creates (or overwrites) /mnt/antiX/etc/minstall.conf and saves a copy to /etc/minstalled.conf for future use.
The installer will not write any passwords or ignored settings to the new configuration file.
Please note, this is experimental. Future installer versions may break compatibility with existing configuration files.</source>
        <translation>Carregar um ficheiro de configuração conforme especificado por &lt;config-file&gt;
Por predefinição será utilizado /etc/minstall.conf.
Esta configuração pode ser utilizada com --auto para uma instalação não vigiada.
O instalador cria (ou reescreve) /mnt/antiX/etc/minstall.conf e grava uma cópia em /etc/minstalled.conf para uso futuro.
O instalador não escreverá no novo ficheiro de configuração quaisquer senhas nem definições ignoradas.
Notar que isto é experimental. Futuras versões do instalador poderão não ser compatíveis com ficheiros de configuração existentes.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="76"/>
        <source>Shutdowns automatically when done installing.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../app.cpp" line="77"/>
        <source>Always use GPT when doing a whole-drive installation regardlesss of capacity.
Without this option, GPT will only be used on drives with at least 2TB capacity.
GPT is always used on whole-drive installations on UEFI systems regardless of capacity, even without this option.</source>
        <translation>Usar sempre a modalidade GPT quando estiver a ser feita uma instalação no disco todo, independentemente da sua capacidade.
Sem esta opção, apenas será utilizada a modalidade GPT em unidades com pelo menos 2 TB de capacidade.
A modalidade GPT é sempre utilizada em instalações no disco todo em sistemas UEFI, independentemente da sua capacidade, mesmo sem esta opção.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="80"/>
        <source>Do not unmount /mnt/antiX or close any of the associated LUKS containers when finished.</source>
        <translation>Quando terminado, não desmontar /mnt/antiX nem fechar qualquer dos contentores LUKS associados.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="81"/>
        <source>Another testing mode for installer, partitions/drives are going to be FORMATED, it will skip copying the files.</source>
        <translation>Outro modo de teste do instalador, partições/unidades serão FORMATADAS, será ignorado o passo de cópia de ficheiros.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="82"/>
        <source>Install the operating system, delaying prompts for user-specific options until the first reboot.
Upon rebooting, the installer will be run with --oobe so that the user can provide these details.
This is useful for OEM installations, selling or giving away a computer with an OS pre-loaded on it.</source>
        <translation>Instalar o sistema operativo, adiando as intervenções do utilizador até após o primeiro reinicio.
Após o reinicio, o instalador irá ser executado com --oobe para que o utilizador possa fornecer as informações.
Isto é útil para instalações de fábrica (OEM), venda ou doação de um computador com um SO pré-carregado.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="85"/>
        <source>Out Of the Box Experience option.
This will start automatically if installed with --oem option.</source>
        <translation>Opção por Experiência «Pronto a Usar».
Isto irá iniciar automaticamente, se instalado com a opção --oem.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="87"/>
        <source>Test mode for GUI, you can advance to different screens without actially installing.</source>
        <translation>Modo de teste da Interface Gráfica; os quadros podem ir sendo percorridos, sem que nada seja de facto instalado.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="88"/>
        <source>Reboots automatically when done installing.</source>
        <translation type="unfinished"/>
    </message>
    <message>
        <location filename="../app.cpp" line="89"/>
        <source>Installing with rsync instead of cp on custom partitioning.
-- doesn&apos;t format /root and it doesn&apos;t work with encryption.</source>
        <translation>Em particionamento personalizado, instalar com rsync e não com cp.
-- não formata /root e não funciona com encriptação.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="91"/>
        <source>Load a configuration file as specified by &lt;config-file&gt;.</source>
        <translation>Carregar um ficheiro de configuração como especificado em &lt;config-file&gt;.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="95"/>
        <source>Too many arguments. Please check the command format by running the program with --help</source>
        <translation>Demasiados argumentos. Verifique o formato do comando ao executar o programa com --help</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="100"/>
        <source>%1 Installer</source>
        <translation>Instalador do %1</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="108"/>
        <source>The installer won't launch because it appears to be running already in the background.

Please close it if possible, or run &apos;pkill minstall&apos; in terminal.</source>
        <translation>O instalador não inicia porque parece já estar em execução em segundo plano.

Encerrar a instância em execução, se possível; se não, no terminal executar &apos;pkill minstall&apos;.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="115"/>
        <source>This operation requires root access.</source>
        <translation>Esta operação necessita de acesso root.</translation>
    </message>
    <message>
        <location filename="../app.cpp" line="136"/>
        <source>Configuration file (%1) not found.</source>
        <translation>Ficheiro de configuração (%1) não encontrado.</translation>
    </message>
</context>
</TS>