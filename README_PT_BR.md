<h1>mGBA</h1>

<p>O mGBA é um emulador para executar jogos de Game Boy Advance. Ele visa ser mais rápido e preciso do que muitos emuladores existentes de Game Boy Advance, assim como adicionar características que faltam em outros emuladores. Ele também suporta jogos de Game Boy e Game Boy Color.</p>

<p>Notícias e downloads atualizados podem ser achados em <a href="https://mgba.io/">mgba.io</a>.</p>

<p><a href="https://buildbot.mgba.io"><img src="https://buildbot.mgba.io/badges/build-win32.svg" alt="Build status" title="" /></a>
<a href="https://hosted.weblate.org/engage/mgba"><img src="https://hosted.weblate.org/widgets/mgba/-/svg-badge.svg" alt="Translation status" title="" /></a></p>

<h2>Características</h2>

<ul>
<li>Suporte altamente preciso ao hardware do Game Boy Advance<a href="#missing"><sup>[1]</sup></a>.</li>
<li>Suporte ao hardware do Game Boy/Game Boy Color.</li>
<li>Emulação rápida. Conhecido por executar na velocidade máxima até mesmo nos hardwares low-end tais como netbooks.</li>
<li>Portes do Qt e SDL pra um frontend peso-pesado e um peso-leve.</li>
<li>Suporte pro cabo do link local (do mesmo computador).</li>
<li>Detecção do tipo do save até mesmo pro tamanho da memória do flash<a href="#flashdetect"><sup>[2]</sup></a>.</li>
<li>Suporte pra cartuchos com sensores de movimento e pro rumble (só usável com controles do jogo).</li>
<li>Suporte pra relógio em tempo real até mesmo sem configuração.</li>
<li>Suporte pra sensor solar pros jogos Boktai.</li>
<li>Suporte pra Câmera do Game Boy e pra Impressora do Game Boy.</li>
<li>Uma implementação embutida da BIOS e a habilidade de carregar arquivos externos da BIOS.</li>
<li>Suporte pra turbo/avanço rápido segurando o Tab.</li>
<li>Rebobinação segurando o Backquote.</li>
<li>Frameskip, configurável até 10.</li>
<li>Suporte pra screenshot.</li>
<li>Suporte pra códigos de trapaça.</li>
<li>9 slots pra savestates. Os savestates são também visíveis como screenshots.</li>
<li>Gravação em Vídeo, GIF, WebP e APNG.</li>
<li>Suporte pra e-Reader.</li>
<li>Controles remapeáveis pra ambos, o teclado e o gamepad.</li>
<li>Carregar dos arquivos ZIP e 7z.</li>
<li>Suporte pros patches IPS, UPS e BPS.</li>
<li>Debugging do jogo via interface da linha do comando e suporte pro GDB remoto compatível com o IDA Pro.</li>
<li>Rebobinação configurável da emulação.</li>
<li>Suporte pro carregamento e exportação dos snapshots do GameShark e Action Replay.</li>
<li>Cores disponíveis pro RetroArch/Libretro e OpenEmu.</li>
<li>Traduções fornecidas pela comunidade pra vários idiomas via <a href="https://hosted.weblate.org/engage/mgba">Weblate</a>.</li>
<li>Muitas, muitas coisas menores.</li>
</ul>

<h4>Mappers do Game Boy</h4>

<p>Os seguintes mappers são totalmente suportados:</p>

<ul>
<li>MBC1</li>
<li>MBC1M</li>
<li>MBC2</li>
<li>MBC3</li>
<li>MBC3+RTC</li>
<li>MBC5</li>
<li>MBC5+Rumble</li>
<li>MBC7</li>
<li>Wisdom Tree (não licenciado)</li>
<li>Pokémon Jade/Diamond (não licenciado)</li>
<li>BBD (não licenciado tipo MBC5)</li>
<li>Hitek (não licenciado tipo MBC5)</li>
</ul>

<p>Os seguintes mappers são suportados parcialmente:</p>

<ul>
<li>MBC6 (suporte pra gravação da memória flash ausente)</li>
<li>MMM01</li>
<li>Pocket Cam</li>
<li>TAMA5 (Suporte pro RTC ausente)</li>
<li>HuC-1 (Suporte pro IR ausente)</li>
<li>HuC-3 (Suporte pro RTC e IR ausente)</li>
</ul>

<h3>Características planejadas</h3>

<ul>
<li>Suporte pro cabo do link do multiplayer em rede.</li>
<li>Suporte pro cabo do link do barramento do Dolphin/JOY.</li>
<li>Mixagem de áudio do MP2k pra som de qualidade maior do que do hardware.</li>
<li>Suporte pra re-gravação das execuções de assistência das ferramentas.</li>
<li>Suporte do Lua pra scripting.</li>
<li>Uma suíte de debug abrangente.</li>
<li>Suporte pra adaptador wireless.</li>
</ul>

<h2>Plataformas suportadas</h2>

<ul>
<li>Windows 7 ou mais novo</li>
<li>OS X 10.8 (Mountain Lion)<a href="#osxver"><sup>[3]</sup></a> ou mais novo</li>
<li>Linux</li>
<li>FreeBSD</li>
<li>Nintendo 3DS</li>
<li>Nintendo Switch</li>
<li>Wii</li>
<li>PlayStation Vita</li>
</ul>

<p>É conhecido que outras plataformas tipo Unix tais como o OpenBSD funcionam bem mas não são testadas e não são totalmente suportadas.</p>

<h3>Requerimentos do sistema</h3>

<p>Os requerimentos são mínimos. Qualquer computador que consiga executar o Windows Vista ou mais novo deve ser capaz de lidar com a emulação. Suporte pra OpenGL 1.1 ou mais novo também é requerido, com o OpenGL 3.2 ou mais novo pros shaders e funções avançadas.</p>

<h2>Downloads</h2>

<p>Os Downloads podem ser achados no site oficial da web na seção dos <a href="http://mgba.io/downloads.html">Downloads</a>. O código fonte pode ser achado no <a href="https://github.com/mgba-emu/mgba/">GitHub</a>.</p>

<h2>Controles</h2>

<p>Os controles são configuráveis no menu das configurações. Muitos controles dos jogos devem ser mapeados automaticamente por padrão. Os controles padrão do teclado são como segue:</p>

<ul>
<li><strong>A</strong>: X</li>
<li><strong>B</strong>: Z</li>
<li><strong>L</strong>: A</li>
<li><strong>R</strong>: S</li>
<li><strong>Start</strong>: Enter</li>
<li><strong>Select</strong>: Backspace</li>
</ul>

<h2>Compilação</h2>

<p>A compilação requer o uso do CMake 3.1 ou mais novo. O GCC e o Clang são ambos conhecidos por funcionar pra compilar o mGBA mas o Visual Studio 2013 e mais antigos são conhecidos por não funcionar. Suporte pro Visual Studio 2015 e mais novo está vindo em breve.</p>

<h4>Construção do Docker</h4>

<p>O jeito recomendado de contruir pra maioria das plataformas é usar o Docker. Várias imagens do Docker são fornecidas que contém a cadeia e as dependências das ferramentas requisitadas pra construir o mGBA através de várias plataformas.</p>

<p>Pra usar uma imagem do Docker pra construir o mGBA simplesmente execute o seguinte comando enquanto na raiz de uma verificação do mGBA:</p>

<pre><code>docker run --rm -t -v $PWD:/home/mgba/src mgba/windows:w32
</code></pre>

<p>Isto produzirá um diretório <code>build-win32</code> com os produtos do build. Substitua o <code>mgba/windows:w32</code> com outra imagem do Docker pra outras plataformas a qual produzirá um outro diretório correspondente. As seguintes imagens do Docker disponíveis no Hub do Docker:</p>

<ul>
<li>mgba/3ds</li>
<li>mgba/switch</li>
<li>mgba/ubuntu:xenial</li>
<li>mgba/ubuntu:bionic</li>
<li>mgba/ubuntu:focal</li>
<li>mgba/ubuntu:groovy</li>
<li>mgba/vita</li>
<li>mgba/wii</li>
<li>mgba/windows:w32</li>
<li>mgba/windows:w64</li>
</ul>

<h4>Construção do *nix</h4>

<p>Pra usar o CMake pra construir num sistema baseado no Unix os comandos recomendados são como segue:</p>

<pre><code>mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
make
sudo make install
</code></pre>

<p>Isto construirá e instalará o mGBA no <code>/usr/bin</code> e <code>/usr/lib</code>. As dependências que estão instaladas serão detectadas automaticamente e as funções que são desativadas se as dependências não são achadas serão mostradas após executar o comando <code>cmake</code> após os avisos sobre ser incapaz de achá-los.</p>

<p>Se você está no macOS os passos são um pouco diferentes. Assumindo que você está usando o gerenciador de pacotes do homebrew os comandos recomendados pra obter as dependências e construir são:</p>

<pre><code>brew install cmake ffmpeg libzip qt5 sdl2 libedit pkg-config
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=`brew --prefix qt5` ..
make
</code></pre>

<p>Note que você não deve fazer um <code>make install</code> no macOS como não funcionará apropriadamente.</p>

<h4>Construção do desenvolvedor do Windows</h4>

<h5>MSYS2</h5>

<p>Pra construir no Windows pra desenvolvimento usar o MSYS2 é recomendado. Siga os passos da instalação achados no site da web deles <a href="https://msys2.github.io">website</a>. Tenha certeza que você está executando a versão de 32 bits ("MSYS2 MinGW 32-bit") (ou a versão de 64 bits "MSYS2 MinGW 64-bit" se você quer construir pro x86_64) e execute este comando adicional (incluindo as chaves) pra instalar as dependências necessárias (por favor note que isto envolve baixar mais de 1100 MBs de pacotes, então levará um longo tempo):</p>

<pre><code>pacman -Sy --needed base-devel git ${MINGW_PACKAGE_PREFIX}-{cmake,ffmpeg,gcc,gdb,libelf,libepoxy,libzip,pkgconf,qt5,SDL2,ntldd-git}
</code></pre>

<p>Verifique o código fonte executando este comando:</p>

<pre><code>git clone https://github.com/mgba-emu/mgba.git
</code></pre>

<p>Então finalmente construa executando estes comandos:</p>

<pre><code>mkdir -p mgba/build
cd mgba/build
cmake .. -G "MSYS Makefiles"
make -j$(nproc --ignore=1)
</code></pre>

<p>Por favor note que este build do mGBA pro Windows não é adequado pra distribuição devido ao espalhamento das DLLs que precisa executar mas é perfeito pro desenvolvimento. Contudo se distribuir tal build é desejado (ex: pra testar em máquinas que não tem o ambiente do MSYS2 instalado) executar o <code>cpack -G ZIP</code> preparará um arquivo zip com todas as DLLs necessárias.</p>

<h5>Visual Studio</h5>

<p>Construir usando o Visual Studio é uma instalação igualmente complicada. Pra começar você precisará instalar o <a href="https://github.com/Microsoft/vcpkg">vcpkg</a>. Após instalar o vcpkg você precisará instalar vários pacotes adicionais:</p>

<pre><code>vcpkg install ffmpeg[vpx,x264] libepoxy libpng libzip sdl2 sqlite3
</code></pre>

<p>Note que esta instalação não suportará codificação de vídeo acelerado por hardware no hardware da Nvidia. Se você se importa com isto você precisará instalar o CUDA antes e então subtitua o <code>ffmpeg[vpx,x264,nvcodec]</code> no comando anterior.</p>

<p>Você também precisará instalar o Qt. Infelizmente devido ao Qt sendo possuído e comandado por uma companhia em dificuldade em oposição a uma organização razoável que não é mais uma instaladora da edição de código fonte aberto offline pra versão mais recente então você precisará ou retroceder pra um <a href="https://download.qt.io/official_releases/qt/5.12/5.12.9/qt-opensource-windows-x86-5.12.9.exe">instalador da versão antiga</a> (a qual quer que você crie uma conta de outro modo inútil mas você pode pular temporariamente configurando um proxy inválido ou de outro modo desative a rede), use o instalador online (o qual requer uma conta apesar de tudo) ou use o vcpkg pra construí-lo (lentamente). Nenhuma destas são grande opções. Para o instalador você vai querer instalar as versões aplicáveis do MSVC. Note que os instaladores offline não suportam o MSVC 2019. Pro vcpkg você vai querer instalá-lo como tal o que levará algum tempo, especialmente no quad core ou computadores inferiores:</p>

<pre><code>vcpkg install qt5-base qt5-multimedia
</code></pre>

<p>Em seguinda abra o Visual Studio, selecione o Repositório dos Clones e entre em <code>https://github.com/mgba-emu/mgba.git</code>. Quando o Visual Studio acabar de clonar vá em Arquivo > CMake e abra o arquivo CMakeLists.txt na raiz do repositório verificado. De lá o mGBA pode ser desenvolvido no Visual Studio de modo igual a outros projetos do CMake no Visual Studio.</p>

<h4>Construção da cadeia de ferramentas</h4>

<p>Se você tem o devkitARM (pro 3DS), devkitPPC (pro Wii), devkitA64 (pro Switch) ou vitasdk (pro PS Vita), você pode usar os seguintes comandos pra construir:</p>

<pre><code>mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../src/platform/3ds/CMakeToolchain.txt ..
make
</code></pre>

<p>Substitua o parâmetro <code>-DCMAKE_TOOLCHAIN_FILE</code> pras seguintes plataformas:</p>

<ul>
<li>3DS: <code>../src/platform/3ds/CMakeToolchain.txt</code></li>
<li>Switch: <code>../src/platform/switch/CMakeToolchain.txt</code></li>
<li>Vita: <code>../src/platform/psp2/CMakeToolchain.vitasdk</code></li>
<li>Wii: <code>../src/platform/wii/CMakeToolchain.txt</code></li>
</ul>

<h3>Dependências</h3>

<p>O mGBA não tem dependências rígidas, contudo as seguintes dependências opcionais são requeridas pra funções específicas. As funções serão desativadas se as dependências não podem ser achadas.</p>

<ul>
<li>Qt 5: para o frontend da GUI. O Qt Multimedia ou SDL são requeridos pro áudio.</li>
<li>SDL: pra um frontend mais básico e suporte pro gamepad no frontend do Qt. O SDL 2 é recomendado mas o 1.2 é suportado.</li>
<li>zlib and libpng: pra suporte a screenshots e suporte pra savestate-in-PNG.</li>
<li>libedit: pra suporte ao debugger de linha de comando.</li>
<li>ffmpeg or libav: pra gravação de vídeo, GIF, WebP e APNG.</li>
<li>libzip or zlib: pra carregar ROMs armazenadas nos arquivos zip.</li>
<li>SQLite3: pros bancos de dados dos jogos.</li>
<li>libelf: pro carregamento do ELF.</li>
</ul>

<p>O SQLite3, libpng e zlib estão incluídos com o emulador então eles não precisam ser compilados externamente primeiro.</p>

<h2>Notas do Rodapé</h2>

<p><a name="missing">[1]</a> As características atualmente ausentes são</p>

<ul>
<li>A janela do OBJ pros modos 3, 4 e 5 (<a href="http://mgba.io/b/5">Bug #5</a>)</li>
</ul>

<p><a name="flashdetect">[2]</a> A detecção do tamanho da memória flash não funciona em alguns casos. Estes podem ser configurados no runtime mas é recomendado reportar um bug se tal caso for encontrado.</p>

<p><a name="osxver">[3]</a> A 10.8 só é necessária para o porte do Qt. Pode ser possível contruir ou executar o porte do Qt na 10.7 ou mais antiga mas isto não é suportado oficialmente. É conhecido que o porte do SDL funciona na 10.5 e pode funcionar nas mais antigas.</p>

<h2>Copyright</h2>

<p>O mGBA tem um Copyright de © 2013 – 2021 do Jeffrey Pfau. É distribuído sob a <a href="https://www.mozilla.org/MPL/2.0/">Licença Pública do Mozilla, versão 2.0</a>. Uma cópia da licença está disponível no arquivo distribuído da LICENÇA.</p>

<p>O mGBA contém as seguintes bibliotecas de terceiros:</p>

<ul>
<li><a href="https://github.com/benhoyt/inih">inih</a>, a qual tem um copyright de © 2009 – 2020 do Ben Hoyt e é usada sob uma licença de 3 claúsulas do BSD.</li>
<li><a href="https://code.google.com/archive/p/blip-buf">blip-buf</a>, a qual tem um copyright de © 2003 – 2009 do Shay Green e é usada sob uma Licença Pública Inferior do GNU.</li>
<li><a href="http://www.7-zip.org/sdk.html">SDK do LZMA</a>, a qual é de domínio público.</li>
<li><a href="https://github.com/aappleby/smhasher">MurmurHash3</a> implementação do Austin Appleby, a qual é de domínio público.</li>
<li><a href="https://github.com/skandhurkat/Getopt-for-Visual-Studio/">getopt pro MSVC</a>, a qual é de domínio público.</li>
<li><a href="https://www.sqlite.org">SQLite3</a>, a qual é de domínio público.</li>
</ul>

<p>Se você é um publicador de jogos e desejar licenciar o mGBA pra uso comercial por favor envie um email pra <a href="mailto:licensing@mgba.io">licensing@mgba.io</a> pra mais informações.</p>
