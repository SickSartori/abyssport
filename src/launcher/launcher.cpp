// abyssport's launcher: the first screen, before the game starts. Whether the game is ready, the settings, the
// controls and how the console's touch screen, rear pad and gyroscope are played on a PC, version and credits; in
// the game's colours, over its own artwork read from the player's copy. Play starts the game (abyss.exe) and the
// launcher comes back when it ends.
//
// The look, the layout and the widgets are those of mc3port's launcher (src/pc/launcher_ui.cpp, MIT, (c) 2026
// SickSartori), on SDL3 and its GPU API instead of Direct3D 11.
//
// For scripts: ABYSS_LAUNCHER_SHOT=<file.png> saves the window's picture once the opening animation is over and
// quits; ABYSS_LAUNCHER_PAGE=0..3 and ABYSS_LAUNCHER_TAB=n choose what it shows. ABYSS_LAUNCHER_AUTOPLAY=<seconds>
// presses Play by itself and quits when the game ends. ABYSS_LAUNCHER_PREPARE=<copy>|<PUP> (a release) chooses the
// two, presses Prepare, and quits when it ends: exit code 0 when the game is then ready. ABYSS_LAUNCHER_LANGUAGE=
// en|it|pt|es speaks that language. ABYSS_LAUNCHER_PLAY=1 (RecompLauncher) presses Play once the game is ready.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlgpu3.h"
#include "imgui.h"
#include "imgui_internal.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb/stb_image_write.h"

extern "C" {
#include "settings.h"
}

#ifndef ABYSS_VERSION
#define ABYSS_VERSION "0.0.0"
#endif

namespace {

namespace fs = std::filesystem;

// --- texts ---

struct Texts {
    const char *play, *settings, *controls, *about, *quit;
    const char *ready_title, *ready_lead, *setup_title, *setup_lead;
    const char *game_ok, *game_missing, *game_missing_detail, *program_missing, *program_missing_detail;
    const char *pad_on, *pad_on_detail, *pad_off, *pad_off_detail;
    const char *style, *style_names[3], *style_details[3], *change;
    const char *tab_picture, *tab_effects, *tab_game, *tab_audio;
    const char *aspect, *aspect_detail, *aspect_names[4];
    const char *window_mode, *window_detail, *window_names[2];
    const char *resolution, *resolution_detail;
    const char *frame_rate, *frame_rate_detail;
    const char *anisotropy;
    const char *fsr, *fsr_detail, *post_aa, *post_aa_detail, *sharpen, *ssao, *ssao_detail, *shadows, *shadow_names[3];
    const char *fov, *fov_detail, *view_distance, *view_distance_detail, *language, *language_names[5];
    const char *volume, *no, *yes;
    const char *controls_lead, *tab_keys, *tab_mouse, *tab_how;
    const char *press_key, *default_keys, *none;
    const char *input_mode, *input_mode_detail, *input_names[3];
    const char *mouse_sens, *mouse_invert, *shoulders, *shoulders_detail, *shoulder_names[2], *gyro, *gyro_detail;
    const char *mouse_lead, *mouse_buttons[5];
    const char *how_lead, *how_devices[3];
    // "How to play": the scenes' names, and what each says for keyboard and mouse, a DualSense, an Xbox controller.
    // {T} {TR}: what holds the front / rear touch cursor; {A} {F}: aim, fire (they swap with "L and R buttons")
    const char *how_titles[9], *how_text[3][9];
    const char *actions[16];
    const char *about_lead, *version, *licence, *licence_text, *folder, *open_saves;
    const char *bar_game, *bar_game_ok, *bar_game_none, *bar_pad, *bar_pad_on, *bar_pad_off, *bar_version;
    const char *move_hint, *select, *back, *pages_hint, *tabs_hint;
    const char *starting;
    // the first run: the game made from the player's copy (tools/make_game.py, tools/build_game.py)
    const char *copy, *copy_none, *pup, *pup_none, *choose, *prepare, *cancel, *failed, *space, *duration, *compile_only;
    const char *steps[7];
    const char *enter;  // the key's name in the hints
};

const Texts kItalian = {
    "GIOCA", "IMPOSTAZIONI", "CONTROLLI", "INFO", "ESCI",
    "PRONTO A PARTIRE", "La giungla di Panama ti aspetta.", "PRIMA DI PARTIRE", "Manca ancora qualcosa per giocare.",
    "Gioco pronto", "Gioco da preparare",
    "Esegui tools/make_game.py con la tua copia del gioco e il file del firmware",
    "Programma del gioco non trovato", "abyss.exe deve stare accanto al launcher",
    "Controller collegato", "Riconosciuto e mappato da solo", "Nessun controller", "Tastiera e mouse funzionano",
    "Stile", {"Originale", "Migliorata", "Personalizzata"},
    {"Come sulla console: 30 fps, risoluzione nativa", "Risoluzione dello schermo, filtri e ombre migliori", "Le tue scelte"},
    "Modifica",
    "Immagine", "Effetti", "Gioco", "Audio",
    "Formato", "Più largo: il gioco mostra più scena ai lati", {"16:9", "21:9", "32:9", "Finestra"},
    "Modalità finestra", "F11 durante il gioco", {"Finestra", "Schermo intero"},
    "Risoluzione interna", "Bordi più lisci, più lavoro per la scheda video",
    "Fotogrammi al secondo", "30 come sulla console; 60 è sperimentale",
    "Filtro delle texture",
    "FSR 3.1", "Anti-aliasing e ricostruzione di AMD", "Bordi morbidi", "Anti-aliasing sull'immagine finita", "Nitidezza",
    "Occlusione ambientale", "Ombre morbide negli angoli e nei contatti", "Ombre", {"Originali", "Morbide", "Più morbide"},
    "Campo visivo", "1.00 è quello del gioco", "Distanza visiva", "Rocce e vegetazione spariscono più lontano",
    "Lingua del launcher", {"Automatica", "Italiano", "English", "Português", "Español"},
    "Volume", "No", "Sì",
    "I controller funzionano così come sono. Qui scegli tasti e mouse, e vedi come si gioca senza touch.",
    "Tastiera", "Mouse e controller", "Come si gioca",
    "Premi un tasto...", "Tasti predefiniti", "nessuno",
    "Comandi", "Automatico segue l'ultimo dispositivo usato", {"Automatico", "Controller", "Tastiera e mouse"},
    "Sensibilità del mouse", "Mouse invertito", "Tasti L e R", "Sui grilletti o sui dorsali del controller",
    {"Grilletti", "Dorsali"}, "Giroscopio del controller", "DualShock 4, DualSense e Switch Pro: inclinano come la console",
    "Con il mouse catturato i suoi pulsanti sono azioni. Clic per cambiare, clic destro per tornare indietro.",
    {"Pulsante sinistro", "Pulsante destro", "Pulsante centrale", "Laterale 1", "Laterale 2"},
    "Il gioco è nato per schermo touch, touch posteriore, giroscopio e fotocamera. Scegli con cosa giochi: ogni scena mostra come si fa.",
    {"Tastiera e mouse", "DualSense", "Xbox"},
    {"Guardarsi attorno", "Toccare lo schermo", "Touch posteriore", "Strofinare, disegnare", "Scorrere",
     "Inclinare", "Lampada della fotocamera", "Mirare e sparare", "Menu del port"},
    {{"Muovi il mouse: il gioco lo cattura e la visuale lo segue.",
      "Tieni premuto Alt: il cursore diventa un dito. Il pulsante sinistro tocca lo schermo.",
      "Con Alt premuto, il pulsante destro tocca il retro della console.",
      "Con Alt premuto trascina il cursore. Le forme disegnate col mouse sono accettate con più tolleranza.",
      "Quando il gioco chiede di scorrere, trascina col pulsante sinistro, anche a mouse catturato.",
      "Su travi e scivoli tieni {ML} o {MR}: la console si inclina da quel lato e torna dritta quando lasci.",
      "F7 accende e spegne la lampada che il gioco vede con la fotocamera posteriore.",
      "Per mirare: {AIM}. Per sparare: {FIRE}. Si cambiano nelle schede Tastiera e Mouse e controller.",
      "F1 apre impostazioni e comandi durante il gioco. F2 cambia risoluzione, F4 l'FSR."},
     {"Levetta destra.",
      "Tocca il touchpad: è lo schermo della console. Oppure tieni {T}, muovi il cursore con una levetta e premi Croce.",
      "Tieni {TR} e tocca il touchpad: tocchi il retro. Oppure tieni {TR}, muovi una levetta e premi Croce.",
      "Trascina il dito sul touchpad. Oppure tieni {T} e Croce e muovi la levetta.",
      "Quando il gioco chiede di scorrere, dai un colpo alla levetta destra in quella direzione.",
      "Inclina il controller: il giroscopio fa da console. R3 rimette dritto.",
      "F7 sulla tastiera: sul controller non c'è un tasto libero per la lampada.",
      "{A} per mirare, {F} per sparare.",
      "Tasto PS, oppure Create e Options insieme."},
     {"Levetta destra.",
      "Tieni {T}: compare il cursore. Muovilo con una levetta e premi A per toccare.",
      "Tieni {TR}: il cursore passa sul retro. Muovilo con una levetta e premi A per toccare.",
      "Tieni {T} e A e muovi la levetta: il cursore disegna.",
      "Quando il gioco chiede di scorrere, dai un colpo alla levetta destra in quella direzione.",
      "Senza giroscopio si inclina con la levetta sinistra, a destra e a sinistra.",
      "F7 sulla tastiera: sul controller non c'è un tasto libero per la lampada.",
      "{A} per mirare, {F} per sparare.",
      "Tasto Xbox, oppure Visualizza e Menu insieme."}},
    {"Avanti (levetta sinistra)", "Indietro", "Sinistra", "Destra", "Croce · salta, arrampicati", "Cerchio · copertura, rotolata",
     "Quadrato · corpo a corpo", "Triangolo · raccogli, interagisci", "L · mira", "R · spara", "Start · pausa", "Select",
     "Croce direzionale su", "Croce direzionale giù", "Croce direzionale sinistra", "Croce direzionale destra"},
    "Uncharted: Golden Abyss ricompilato per PC. Serve la tua copia del gioco: niente del gioco viene distribuito.",
    "Versione", "Licenza", "GNU General Public License, versione 2 o successiva",
    "Salvataggi", "Apri la cartella",
    "Gioco", "pronto", "da preparare", "Controller", "collegato", "non collegato", "Versione",
    "Muovi", "Seleziona", "Indietro", "Pagine", "Schede",
    "Avvio del gioco...",
    "La tua copia del gioco", "La cartella PCSA00029 copiata dalla console, con sce_sys/package/work.bin",
    "Il file del firmware", "PSVUPDAT.PUP, aggiornamento 3.74 di PS Vita",
    "Scegli", "PREPARA", "Annulla", "Preparazione non riuscita",
    "Servono almeno %d GB liberi in %s",
    "Una volta sola: circa 15 minuti. Il gioco viene decrittato, ricompilato e compilato su questo PC.",
    "Il gioco è pronto ma va compilato per questa versione del port: circa 6 minuti.",
    {"Decrittazione del firmware", "Decrittazione del gioco", "Controllo della versione 1.00", "Analisi del codice",
     "Codice ARM in C", "Compilazione", "Collegamento"},
    "Invio",
};

const Texts kEnglish = {
    "PLAY", "SETTINGS", "CONTROLS", "ABOUT", "QUIT",
    "READY TO GO", "The jungle of Panama is waiting.", "BEFORE YOU START", "Something is still missing to play.",
    "Game ready", "Game to be made",
    "Run tools/make_game.py with your copy of the game and the firmware file",
    "The game's program was not found", "abyss.exe must sit beside the launcher",
    "Controller connected", "Recognised and mapped by itself", "No controller", "Keyboard and mouse work",
    "Style", {"Original", "Enhanced", "Custom"},
    {"As on the console: 30 fps, native resolution", "The screen's resolution, better filters and shadows", "Your own choices"},
    "Change",
    "Picture", "Effects", "Game", "Audio",
    "Aspect ratio", "Wider: the game shows more of the scene at the sides", {"16:9", "21:9", "32:9", "Window"},
    "Window mode", "F11 during the game", {"Window", "Full screen"},
    "Internal resolution", "Smoother edges, more work for the graphics card",
    "Frames per second", "30 as on the console; 60 is experimental",
    "Texture filtering",
    "FSR 3.1", "AMD's anti-aliasing and reconstruction", "Soft edges", "Anti-aliasing on the finished picture", "Sharpness",
    "Ambient occlusion", "Soft shadows in corners and contacts", "Shadows", {"Original", "Soft", "Softer"},
    "Field of view", "1.00 is the game's own", "View distance", "Rocks and foliage fade out further away",
    "Launcher language", {"Automatic", "Italiano", "English", "Português", "Español"},
    "Volume", "No", "Yes",
    "Controllers work as they are. Here you choose keys and mouse, and see how to play without touch.",
    "Keyboard", "Mouse and controller", "How to play",
    "Press a key...", "Default keys", "none",
    "Controls", "Automatic follows the device used last", {"Automatic", "Controller", "Keyboard and mouse"},
    "Mouse sensitivity", "Inverted mouse", "L and R buttons", "On the controller's triggers or bumpers",
    {"Triggers", "Bumpers"}, "Controller gyroscope", "DualShock 4, DualSense and Switch Pro: they tilt like the console",
    "With the mouse captured its buttons are actions. Click to change, right click to go back.",
    {"Left button", "Right button", "Middle button", "Side button 1", "Side button 2"},
    "The game was made for a touch screen, a rear touch pad, a gyroscope and a camera. Choose what you play with: each scene shows how.",
    {"Keyboard and mouse", "DualSense", "Xbox"},
    {"Looking around", "Touching the screen", "Rear touch pad", "Rubbing, drawing", "Swiping",
     "Tilting", "The camera's lamp", "Aiming and firing", "The port's menu"},
    {{"Move the mouse: the game captures it and the view follows.",
      "Hold Alt: the cursor becomes a finger. The left button touches the screen.",
      "With Alt held, the right button touches the back of the console.",
      "With Alt held, drag the cursor. Shapes drawn with the mouse are accepted more easily.",
      "When the game asks for a swipe, drag with the left button, even with the mouse captured.",
      "On beams and slides hold {ML} or {MR}: the console tilts that way and comes level when you let go.",
      "F7 turns on and off the lamp the game sees through the rear camera.",
      "To aim: {AIM}. To fire: {FIRE}. They change in the Keyboard and the Mouse and controller tabs.",
      "F1 opens settings and controls during the game. F2 changes resolution, F4 FSR."},
     {"Right stick.",
      "Touch the touchpad: it is the console's screen. Or hold {T}, move the cursor with a stick and press Cross.",
      "Hold {TR} and touch the touchpad: you touch the back. Or hold {TR}, move a stick and press Cross.",
      "Drag your finger on the touchpad. Or hold {T} and Cross and move the stick.",
      "When the game asks for a swipe, flick the right stick that way.",
      "Tilt the controller: its gyroscope stands in for the console's. R3 levels it again.",
      "F7 on the keyboard: the controller has no button to spare for the lamp.",
      "{A} to aim, {F} to fire.",
      "The PS button, or Create and Options together."},
     {"Right stick.",
      "Hold {T}: the cursor appears. Move it with a stick and press A to touch.",
      "Hold {TR}: the cursor moves to the back. Move it with a stick and press A to touch.",
      "Hold {T} and A and move the stick: the cursor draws.",
      "When the game asks for a swipe, flick the right stick that way.",
      "Without a gyroscope you tilt with the left stick, left and right.",
      "F7 on the keyboard: the controller has no button to spare for the lamp.",
      "{A} to aim, {F} to fire.",
      "The Xbox button, or View and Menu together."}},
    {"Forward (left stick)", "Back", "Left", "Right", "Cross · jump, climb", "Circle · cover, roll",
     "Square · melee", "Triangle · pick up, interact", "L · aim", "R · fire", "Start · pause", "Select",
     "D-pad up", "D-pad down", "D-pad left", "D-pad right"},
    "Uncharted: Golden Abyss recompiled for PC. You need your own copy of the game: nothing of the game ships.",
    "Version", "Licence", "GNU General Public License, version 2 or later",
    "Saved games", "Open the folder",
    "Game", "ready", "to be made", "Controller", "connected", "not connected", "Version",
    "Move", "Select", "Back", "Pages", "Tabs",
    "Starting the game...",
    "Your copy of the game", "The PCSA00029 folder copied off the console, with sce_sys/package/work.bin",
    "The firmware file", "PSVUPDAT.PUP, PS Vita system update 3.74",
    "Choose", "PREPARE", "Cancel", "Preparing failed",
    "At least %d GB must be free in %s",
    "Once only: about 15 minutes. The game is decrypted, recompiled and compiled on this PC.",
    "The game is ready but must be compiled for this version of the port: about 6 minutes.",
    {"Decrypting the firmware", "Decrypting the game", "Checking version 1.00", "Analysing the code",
     "ARM code to C", "Compiling", "Linking"},
    "Enter",
};

std::string setting(const char* key, const char* def) {
    char buffer[128];
    return settings_str(key, def, buffer, sizeof(buffer));
}

const Texts kPortuguese = {
    "JOGAR", "CONFIGURAÇÕES", "CONTROLES", "SOBRE", "SAIR",
    "PRONTO PARA JOGAR", "A selva do Panamá está esperando.", "ANTES DE COMEÇAR", "Ainda falta algo para jogar.",
    "Jogo pronto", "Jogo a preparar",
    "Execute tools/make_game.py com sua cópia do jogo e o arquivo do firmware",
    "Programa do jogo não encontrado", "abyss.exe deve ficar ao lado do launcher",
    "Controle conectado", "Reconhecido e mapeado sozinho", "Nenhum controle", "Teclado e mouse funcionam",
    "Estilo", {"Original", "Aprimorado", "Personalizado"},
    {"Como no console: 30 fps, resolução nativa", "Resolução da tela, filtros e sombras melhores", "Suas escolhas"},
    "Alterar",
    "Imagem", "Efeitos", "Jogo", "Áudio",
    "Proporção", "Mais larga: o jogo mostra mais da cena nas laterais", {"16:9", "21:9", "32:9", "Janela"},
    "Modo de janela", "F11 durante o jogo", {"Janela", "Tela cheia"},
    "Resolução interna", "Bordas mais suaves, mais trabalho para a placa de vídeo",
    "Quadros por segundo", "30 como no console; 60 é experimental",
    "Filtragem de texturas",
    "FSR 3.1", "Anti-aliasing e reconstrução da AMD", "Bordas suaves", "Anti-aliasing na imagem final", "Nitidez",
    "Oclusão de ambiente", "Sombras suaves em cantos e contatos", "Sombras", {"Originais", "Suaves", "Mais suaves"},
    "Campo de visão", "1.00 é o do jogo", "Distância de visão", "Rochas e vegetação somem mais longe",
    "Idioma do launcher", {"Automático", "Italiano", "English", "Português", "Español"},
    "Volume", "Não", "Sim",
    "Os controles funcionam como estão. Aqui você escolhe teclas e mouse, e vê como jogar sem toque.",
    "Teclado", "Mouse e controle", "Como jogar",
    "Pressione uma tecla...", "Teclas padrão", "nenhuma",
    "Comandos", "Automático segue o último dispositivo usado", {"Automático", "Controle", "Teclado e mouse"},
    "Sensibilidade do mouse", "Mouse invertido", "Botões L e R", "Nos gatilhos ou nos botões de ombro do controle",
    {"Gatilhos", "Ombros"}, "Giroscópio do controle", "DualShock 4, DualSense e Switch Pro: inclinam como o console",
    "Com o mouse capturado, seus botões são ações. Clique para alterar, clique direito para voltar.",
    {"Botão esquerdo", "Botão direito", "Botão do meio", "Lateral 1", "Lateral 2"},
    "O jogo foi feito para tela de toque, touch traseiro, giroscópio e câmera. Escolha com o que você joga: cada cena mostra como.",
    {"Teclado e mouse", "DualSense", "Xbox"},
    {"Olhar ao redor", "Tocar a tela", "Touch traseiro", "Esfregar, desenhar", "Deslizar",
     "Inclinar", "A lâmpada da câmera", "Mirar e atirar", "O menu do port"},
    {{"Mova o mouse: o jogo o captura e a visão o segue.",
      "Segure Alt: o cursor vira um dedo. O botão esquerdo toca a tela.",
      "Com Alt pressionado, o botão direito toca a parte de trás do console.",
      "Com Alt pressionado, arraste o cursor. Formas desenhadas com o mouse são aceitas com mais tolerância.",
      "Quando o jogo pedir para deslizar, arraste com o botão esquerdo, mesmo com o mouse capturado.",
      "Em vigas e rampas segure {ML} ou {MR}: o console se inclina para esse lado e volta reto quando você solta.",
      "F7 liga e desliga a lâmpada que o jogo vê pela câmera traseira.",
      "Para mirar: {AIM}. Para atirar: {FIRE}. Eles mudam nas abas Teclado e Mouse e controle.",
      "F1 abre configurações e comandos durante o jogo. F2 muda a resolução, F4 o FSR."},
     {"Analógico direito.",
      "Toque o touchpad: ele é a tela do console. Ou segure {T}, mova o cursor com um analógico e aperte Xis.",
      "Segure {TR} e toque o touchpad: você toca a parte de trás. Ou segure {TR}, mova um analógico e aperte Xis.",
      "Arraste o dedo no touchpad. Ou segure {T} e Xis e mova o analógico.",
      "Quando o jogo pedir para deslizar, dê um toque no analógico direito nessa direção.",
      "Incline o controle: o giroscópio faz o papel do console. R3 o endireita.",
      "F7 no teclado: o controle não tem um botão livre para a lâmpada.",
      "{A} para mirar, {F} para atirar.",
      "Botão PS, ou Create e Options juntos."},
     {"Analógico direito.",
      "Segure {T}: aparece o cursor. Mova-o com um analógico e aperte A para tocar.",
      "Segure {TR}: o cursor passa para a parte de trás. Mova-o com um analógico e aperte A para tocar.",
      "Segure {T} e A e mova o analógico: o cursor desenha.",
      "Quando o jogo pedir para deslizar, dê um toque no analógico direito nessa direção.",
      "Sem giroscópio, inclina-se com o analógico esquerdo, para a direita e para a esquerda.",
      "F7 no teclado: o controle não tem um botão livre para a lâmpada.",
      "{A} para mirar, {F} para atirar.",
      "Botão Xbox, ou Exibir e Menu juntos."}},
    {"Frente (analógico esquerdo)", "Trás", "Esquerda", "Direita", "Xis · pular, escalar", "Círculo · cobertura, rolar",
     "Quadrado · corpo a corpo", "Triângulo · pegar, interagir", "L · mirar", "R · atirar", "Start · pausa", "Select",
     "Direcional para cima", "Direcional para baixo", "Direcional para a esquerda", "Direcional para a direita"},
    "Uncharted: Golden Abyss recompilado para PC. Você precisa da sua própria cópia do jogo: nada do jogo é distribuído.",
    "Versão", "Licença", "GNU General Public License, versão 2 ou posterior",
    "Jogos salvos", "Abrir a pasta",
    "Jogo", "pronto", "a preparar", "Controle", "conectado", "não conectado", "Versão",
    "Mover", "Selecionar", "Voltar", "Páginas", "Abas",
    "Iniciando o jogo...",
    "Sua cópia do jogo", "A pasta PCSA00029 copiada do console, com sce_sys/package/work.bin",
    "O arquivo do firmware", "PSVUPDAT.PUP, atualização 3.74 do sistema do PS Vita",
    "Escolher", "PREPARAR", "Cancelar", "A preparação falhou",
    "São necessários pelo menos %d GB livres em %s",
    "Só uma vez: cerca de 15 minutos. O jogo é descriptografado, recompilado e compilado neste PC.",
    "O jogo está pronto, mas precisa ser compilado para esta versão do port: cerca de 6 minutos.",
    {"Descriptografando o firmware", "Descriptografando o jogo", "Verificando a versão 1.00", "Analisando o código",
     "Código ARM em C", "Compilando", "Vinculando"},
    "Enter",
};

const Texts kSpanish = {
    "JUGAR", "AJUSTES", "CONTROLES", "ACERCA DE", "SALIR",
    "LISTO PARA JUGAR", "La selva de Panamá te espera.", "ANTES DE EMPEZAR", "Aún falta algo para jugar.",
    "Juego listo", "Juego por preparar",
    "Ejecuta tools/make_game.py con tu copia del juego y el archivo del firmware",
    "No se encontró el programa del juego", "abyss.exe debe estar junto al launcher",
    "Mando conectado", "Reconocido y asignado solo", "Ningún mando", "Teclado y ratón funcionan",
    "Estilo", {"Original", "Mejorado", "Personalizado"},
    {"Como en la consola: 30 fps, resolución nativa", "Resolución de la pantalla, mejores filtros y sombras", "Tus elecciones"},
    "Cambiar",
    "Imagen", "Efectos", "Juego", "Audio",
    "Relación de aspecto", "Más ancha: el juego muestra más escena a los lados", {"16:9", "21:9", "32:9", "Ventana"},
    "Modo de ventana", "F11 durante el juego", {"Ventana", "Pantalla completa"},
    "Resolución interna", "Bordes más suaves, más trabajo para la tarjeta gráfica",
    "Fotogramas por segundo", "30 como en la consola; 60 es experimental",
    "Filtrado de texturas",
    "FSR 3.1", "Antialiasing y reconstrucción de AMD", "Bordes suaves", "Antialiasing sobre la imagen final", "Nitidez",
    "Oclusión ambiental", "Sombras suaves en esquinas y contactos", "Sombras", {"Originales", "Suaves", "Más suaves"},
    "Campo de visión", "1.00 es el del juego", "Distancia de visión", "Rocas y vegetación desaparecen más lejos",
    "Idioma del launcher", {"Automático", "Italiano", "English", "Português", "Español"},
    "Volumen", "No", "Sí",
    "Los mandos funcionan tal cual. Aquí eliges teclas y ratón, y ves cómo se juega sin pantalla táctil.",
    "Teclado", "Ratón y mando", "Cómo se juega",
    "Pulsa una tecla...", "Teclas predeterminadas", "ninguna",
    "Controles", "Automático sigue el último dispositivo usado", {"Automático", "Mando", "Teclado y ratón"},
    "Sensibilidad del ratón", "Ratón invertido", "Botones L y R", "En los gatillos o en los botones superiores del mando",
    {"Gatillos", "Superiores"}, "Giroscopio del mando", "DualShock 4, DualSense y Switch Pro: se inclinan como la consola",
    "Con el ratón capturado sus botones son acciones. Clic para cambiar, clic derecho para volver.",
    {"Botón izquierdo", "Botón derecho", "Botón central", "Lateral 1", "Lateral 2"},
    "El juego se hizo para pantalla táctil, panel táctil trasero, giroscopio y cámara. Elige con qué juegas: cada escena muestra cómo.",
    {"Teclado y ratón", "DualSense", "Xbox"},
    {"Mirar alrededor", "Tocar la pantalla", "Panel táctil trasero", "Frotar, dibujar", "Deslizar",
     "Inclinar", "La lámpara de la cámara", "Apuntar y disparar", "El menú del port"},
    {{"Mueve el ratón: el juego lo captura y la vista lo sigue.",
      "Mantén Alt: el cursor se vuelve un dedo. El botón izquierdo toca la pantalla.",
      "Con Alt pulsado, el botón derecho toca la parte trasera de la consola.",
      "Con Alt pulsado, arrastra el cursor. Las formas dibujadas con el ratón se aceptan con más tolerancia.",
      "Cuando el juego pide deslizar, arrastra con el botón izquierdo, incluso con el ratón capturado.",
      "En vigas y rampas mantén {ML} o {MR}: la consola se inclina hacia ese lado y vuelve recta al soltar.",
      "F7 enciende y apaga la lámpara que el juego ve con la cámara trasera.",
      "Para apuntar: {AIM}. Para disparar: {FIRE}. Se cambian en las pestañas Teclado y Ratón y mando.",
      "F1 abre ajustes y controles durante el juego. F2 cambia la resolución, F4 el FSR."},
     {"Joystick derecho.",
      "Toca el panel táctil: es la pantalla de la consola. O mantén {T}, mueve el cursor con un joystick y pulsa Equis.",
      "Mantén {TR} y toca el panel táctil: tocas la parte trasera. O mantén {TR}, mueve un joystick y pulsa Equis.",
      "Arrastra el dedo por el panel táctil. O mantén {T} y Equis y mueve el joystick.",
      "Cuando el juego pide deslizar, da un toque al joystick derecho en esa dirección.",
      "Inclina el mando: su giroscopio hace de consola. R3 lo endereza.",
      "F7 en el teclado: el mando no tiene un botón libre para la lámpara.",
      "{A} para apuntar, {F} para disparar.",
      "Botón PS, o Create y Options a la vez."},
     {"Joystick derecho.",
      "Mantén {T}: aparece el cursor. Muévelo con un joystick y pulsa A para tocar.",
      "Mantén {TR}: el cursor pasa a la parte trasera. Muévelo con un joystick y pulsa A para tocar.",
      "Mantén {T} y A y mueve el joystick: el cursor dibuja.",
      "Cuando el juego pide deslizar, da un toque al joystick derecho en esa dirección.",
      "Sin giroscopio se inclina con el joystick izquierdo, a derecha e izquierda.",
      "F7 en el teclado: el mando no tiene un botón libre para la lámpara.",
      "{A} para apuntar, {F} para disparar.",
      "Botón Xbox, o Vista y Menú a la vez."}},
    {"Adelante (joystick izquierdo)", "Atrás", "Izquierda", "Derecha", "Equis · saltar, trepar", "Círculo · cobertura, rodar",
     "Cuadrado · cuerpo a cuerpo", "Triángulo · recoger, interactuar", "L · apuntar", "R · disparar", "Start · pausa", "Select",
     "Cruceta arriba", "Cruceta abajo", "Cruceta izquierda", "Cruceta derecha"},
    "Uncharted: Golden Abyss recompilado para PC. Necesitas tu propia copia del juego: no se distribuye nada del juego.",
    "Versión", "Licencia", "GNU General Public License, versión 2 o posterior",
    "Partidas guardadas", "Abrir la carpeta",
    "Juego", "listo", "por preparar", "Mando", "conectado", "no conectado", "Versión",
    "Mover", "Seleccionar", "Atrás", "Páginas", "Pestañas",
    "Iniciando el juego...",
    "Tu copia del juego", "La carpeta PCSA00029 copiada de la consola, con sce_sys/package/work.bin",
    "El archivo del firmware", "PSVUPDAT.PUP, actualización 3.74 del sistema de PS Vita",
    "Elegir", "PREPARAR", "Cancelar", "La preparación falló",
    "Se necesitan al menos %d GB libres en %s",
    "Solo una vez: unos 15 minutos. El juego se descifra, se recompila y se compila en este PC.",
    "El juego está listo, pero debe compilarse para esta versión del port: unos 6 minutos.",
    {"Descifrando el firmware", "Descifrando el juego", "Comprobando la versión 1.00", "Analizando el código",
     "Código ARM a C", "Compilando", "Enlazando"},
    "Intro",
};

// The launcher's language: the setting, else Windows' (Italian, Portuguese, Spanish, else English).
// ABYSS_LAUNCHER_LANGUAGE=en|it|pt|es overrides both, for test screenshots.
const Texts& T() {
    char forced[8] = {};
    std::string language = GetEnvironmentVariableA("ABYSS_LAUNCHER_LANGUAGE", forced, sizeof(forced)) ? forced
                                                                                                    : setting("launcher_language", "auto");
    if (language == "auto") {
        const WORD windows = PRIMARYLANGID(GetUserDefaultUILanguage());
        language = windows == LANG_ITALIAN ? "it" : windows == LANG_PORTUGUESE ? "pt" : windows == LANG_SPANISH ? "es" : "en";
    }
    return language == "it" ? kItalian : language == "pt" ? kPortuguese : language == "es" ? kSpanish : kEnglish;
}

// --- colours and small helpers (mc3port's, the accent gold instead of red) ---

constexpr ImU32 kGold = IM_COL32(226, 168, 44, 255), kGoldDark = IM_COL32(150, 96, 14, 255);
constexpr ImU32 kInk = IM_COL32(242, 241, 238, 255), kDim = IM_COL32(170, 166, 158, 255);
constexpr ImU32 kGreen = IM_COL32(110, 227, 110, 255), kAmber = IM_COL32(255, 178, 30, 255);
constexpr ImU32 kChrome = IM_COL32(217, 218, 223, 255);

float g_alpha = 1;  // the current element's fade

// The launcher's widgets are invisible buttons drawn by hand; ImGui leaves those out of keyboard and controller
// navigation unless asked.
constexpr ImGuiButtonFlags kNavigable = ImGuiButtonFlags_EnableNav;

ImU32 fade(ImU32 colour, float alpha = 1) {
    const float a = static_cast<float>((colour >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(alpha * g_alpha, 0.0f, 1.0f);
    return (colour & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
}
ImU32 mix(ImU32 a, ImU32 b, float t) {
    return ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(a), ImGui::ColorConvertU32ToFloat4(b), t));
}
float ease_out(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1 - (1 - t) * (1 - t) * (1 - t);
}
bool key_pressed(std::initializer_list<ImGuiKey> keys) {
    for (const ImGuiKey key : keys) {
        if (ImGui::IsKeyPressed(key)) return true;
    }
    return false;
}
// Held keys repeat at a menu's pace (0.4 s, then every 0.15 s), not ImGui's 20 a second.
bool menu_key_pressed(std::initializer_list<ImGuiKey> keys) {
    for (const ImGuiKey key : keys) {
        if (ImGui::GetKeyPressedAmount(key, 0.4f, 0.15f) > 0) return true;
    }
    return false;
}
float random01() { return static_cast<float>(std::rand()) / RAND_MAX; }

// --- the game's keys (the same table as the runtime's, src/ui/window.c kb_binds) ---

struct Bind {
    const char *key, *def, *glyph;
};
constexpr Bind kBinds[16] = {
    {"bind_move_up", "W", "LS"},   {"bind_move_down", "S", "LS"}, {"bind_move_left", "A", "LS"}, {"bind_move_right", "D", "LS"},
    {"bind_cross", "Space", "X"},  {"bind_circle", "C", "O"},     {"bind_square", "F", "S"},     {"bind_triangle", "E", "T"},
    {"bind_l", "Q", "L"},          {"bind_r", "R", "R"},          {"bind_start", "Return", "ST"}, {"bind_select", "Tab", "SE"},
    {"bind_up", "Up", "DU"},       {"bind_down", "Down", "DD"},   {"bind_left", "Left", "DL"},   {"bind_right", "Right", "DR"},
};
struct MouseBind {
    const char *key, *def;
};
constexpr MouseBind kMouseBinds[5] = {{"mouse_left", "r"}, {"mouse_right", "l"}, {"mouse_middle", ""}, {"mouse_x1", ""}, {"mouse_x2", ""}};

// --- styles: what Original and Enhanced set (mc3port's launcher_style.h, with text values) ---

struct StyleValue {
    const char *key, *original, *enhanced, *runtime;  // "auto": the resolution that fills the screen; runtime: what the game uses when the key is not set
};
constexpr StyleValue kStyleValues[] = {
    {"frame_rate", "30", "30", "30"},  {"aspect_ratio", "native", "native", "native"}, {"render_scale", "1", "auto", "1"},
    {"anisotropy", "1", "16", "16"},   {"fsr", "0", "0", "0"},                    {"post_aa", "0", "1", "1"},
    {"sharpen", "0", "0.35", "0.35"},    {"ssao", "0", "1", "1"},                   {"shadow_filter", "0", "1", "1"},
};
enum { kStyleOriginal, kStyleEnhanced, kStyleCustom };

struct Trail {
    float x, y, speed, length;
    ImU32 colour;
};

class Launcher {
public:
    int run();

private:
    enum class Page { kPlay, kSettings, kControls, kAbout, kQuit };

    bool start();
    void stop();
    void load_fonts();
    void load_art();
    SDL_GPUTexture* make_texture(const unsigned char* rgba, int width, int height);
    void find_game();
    void start_game();
    fs::path setup_base() const;
    void start_setup();
    bool run_tool(const std::wstring& command, bool making);
    void setup_rows();
    void cancel_setup();
    void frame(float now, float dt);
    void header(float since);
    void panel(float now);
    void side_menu(float now, float since, const char* const* labels, const Page* targets, int count);
    void bottom_bar(float since);
    void after_frame(float now);
    void back_to_menu();
    void leave_page_on_left();
    void claim_focus() {
        if (!focus_panel_) return;
        ImGui::SetKeyboardFocusHere();
        ImGui::SetNavCursorVisible(true);  // without it Enter does nothing until a direction is pressed
        focus_panel_ = false;
    }
    void configure_navigation() {
        constexpr ImGuiConfigFlags kNavigation = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
        ImGuiIO& io = ImGui::GetIO();
        if (menu_has_focus_) io.ConfigFlags &= ~kNavigation; else io.ConfigFlags |= kNavigation;
    }
    bool nav_selected() const { return ImGui::IsItemFocused() && !menu_has_focus_ && ImGui::GetCurrentContext()->NavCursorVisible; }
    void focus_frame(ImVec2 a, ImVec2 b) {
        if (!nav_selected()) return;
        row_marked_ = true;
        row_a_ = a;
        row_b_ = b;
    }
    void focus_ring();
    int value_step();
    bool step_slider(float* value, float minimum, float maximum);
    bool save_shot(SDL_GPUTexture* texture, SDL_GPUTextureFormat format, const char* path);

    // Layout helpers: design pixels (1600x900) times scale_.
    float px(float value) const { return value * scale_; }
    void text(ImFont* font, float size, ImVec2 at, ImU32 colour, const char* str, float wrap = 0) {
        list_->AddText(font, size, at, fade(colour), str, nullptr, wrap);
    }
    ImVec2 measure(ImFont* font, float size, const char* str, float wrap = 0) const { return font->CalcTextSizeA(size, FLT_MAX, wrap, str); }
    void skewed_rect(ImDrawList* list, ImVec2 a, ImVec2 b, ImU32 left, ImU32 right, float skew_ratio = 0.21f);
    float anim(ImGuiID id, bool on, float speed = 12.0f);
    float glyph(const char* name, ImVec2 at, float h);

    // Panel content: cursor_ moves down.
    void title(const char* str);
    void lead(const char* str);
    bool status(bool good, const char* heading, const char* detail, const char* button_label, const char* id, bool pulse = false);
    bool small_button(const char* label, ImVec2 at, const char* id, ImVec2* size_out = nullptr);
    bool big_button(const char* label, const char* id, bool pulse);
    bool tabs(const char* const* labels, int count, int* current);
    void row(const char* heading, const char* detail, float control = 270);
    bool segmented(const char* id, const char* const* labels, int count, int* current, const char* const* details = nullptr, float wide = 0);
    bool slider(const char* id, float* value, float minimum, float maximum, const char* format);
    // ImGui::SliderFloat with its text hidden and the value drawn beside the grab, on the side with more room, so the
    // grab never covers it.
    bool value_slider(const char* id, float* value, float minimum, float maximum, const char* format, float font_size);

    void page_play();
    void page_settings();
    void page_controls();
    void page_about();
    void how_page(float now);
    void style_row();
    int style() const;
    void apply_style(int style);
    std::string auto_scale() const;
    // A setting on a segmented control: the values in `values`, the labels beside them.
    void choice(const char* key, const char* def, const char* const* values, const char* const* labels, int count,
                const char* const* details = nullptr, float wide = 0);
    void number(const char* key, float def, float minimum, float maximum, const char* format);

    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUTexture *background_ = nullptr, *logo_ = nullptr, *emblem_ = nullptr;
    SDL_GPUTexture* pad_art_ = nullptr;   // "how to play": the controller (art/pad.png, tools/art/make_pad.py)
    SDL_GPUTexture* pad_bumpers_ = nullptr;   // ... and its bumpers, to light up (art/pad_bumpers.png)
    bool background_is_game_ = false;   // the game's own picture, not one of ours (art/bg_*.jpg)
    int background_width_ = 0, background_height_ = 0, logo_width_ = 0, logo_height_ = 0;
    int width_ = 0, height_ = 0;
    float scale_ = 1;

    ImFont *body_ = nullptr, *body_bold_ = nullptr, *italic_ = nullptr, *heading_ = nullptr, *black_ = nullptr;

    fs::path exe_directory_, game_directory_, program_;
    bool game_ready_ = false, program_ready_ = false, pad_connected_ = false;
    bool tools_ready_ = false;  // the release: Python, the tools, the runtime and the compiler beside the launcher

    // The first run, on a worker thread: everything under setup_lock_ is shared with it. The dialogs' answers too
    // (SDL calls them back on a thread of its own).
    std::mutex setup_lock_;
    std::thread setup_thread_;
    HANDLE setup_job_ = nullptr;   // the tools and the compilers they start: Cancel or closing the launcher ends them all
    bool setup_running_ = false, setup_done_ = false, setup_cancelled_ = false;
    int setup_step_ = 0;           // Texts::steps
    float setup_progress_ = 0;
    std::string setup_error_, setup_tail_;
    std::string chosen_copy_, chosen_pup_;   // UTF-8
    bool prepare_auto_ = false;              // ABYSS_LAUNCHER_PREPARE: started by a script, which waits for the end
    std::string game_version_;

    bool menu_has_focus_ = true, focus_panel_ = false, left_pending_ = false;
    int menu_cursor_ = 0;
    ImGuiID left_from_ = 0;
    Page page_ = Page::kPlay;
    float page_since_ = 0, start_time_ = 0;
    int settings_tab_ = 0, controls_tab_ = 0, entered_page_frame_ = -1, value_moved_frame_ = -1;
    int hover_item_ = -1;
    float hover_since_ = 0;
    bool hover_armed_ = false;
    int rebinding_ = -1, cancel_frame_ = -1;
    int how_item_ = 0;                             // "how to play": the scene shown
    int how_device_ = -1;                          // ... for: 0 keyboard and mouse, 1 DualSense, 2 Xbox; -1: from the controller connected
    float how_since_ = 0, how_hold_until_ = 0;     // when it began; until when the player's choice stays
    bool done_ = false, play_ = false, status_keeps_focus_ = false;
    bool row_marked_ = false;
    ImVec2 row_a_{}, row_b_{};

    ImDrawList* list_ = nullptr;
    ImVec2 origin_;  // panel content top left
    float panel_width_ = 0, cursor_ = 0, row_extra_ = 0;
    // The page's column runs from the top to the bottom bar; a longer page scrolls in it (the wheel, or the selection
    // kept in sight). view_top_/view_bottom_: the part of the screen the page shows.
    float scroll_ = 0, scroll_target_ = 0, view_top_ = 0, view_bottom_ = 0;
    Page scroll_page_ = Page::kPlay;
    int scroll_tab_ = 0;
    ImGuiID scrolled_to_ = 0;  // the selection last brought into sight
    // The page's items come in one after another: each title, row and button counts one (enter_item).
    void enter_item();
    int item_count_ = 0;
    float item_shift_ = 0, page_alpha_ = 1;
    std::unordered_map<ImGuiID, float> anims_;
    std::vector<Trail> trails_;
    std::vector<ImVec4> motes_;
};

// --- start-up ---

bool Launcher::start() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return false;
    const char* base = SDL_GetBasePath();
    exe_directory_ = base ? fs::path(base) : fs::current_path();

    // The settings: the game's own file (in the folder it runs from) under the shared one, as the runtime reads them.
    find_game();
    if (game_ready_) settings_load((game_directory_ / "settings.ini").string().c_str());
    if (char* pref = SDL_GetPrefPath(nullptr, "abyssport")) {
        settings_load_shared((std::string(pref) + "settings.ini").c_str());
        SDL_free(pref);
    }

    float display_scale = 1;
    SDL_Rect bounds = {0, 0, 1920, 1080};
    if (const SDL_DisplayID display = SDL_GetPrimaryDisplay()) SDL_GetDisplayUsableBounds(display, &bounds);
    int width = 1600, height = 900;
    while (width > bounds.w - 40 || height > bounds.h - 80) {
        width = width * 9 / 10;
        height = height * 9 / 10;
    }
    (void)display_scale;
    const bool shot = std::getenv("ABYSS_LAUNCHER_SHOT") != nullptr;
    window_ = SDL_CreateWindow("abyssport", shot ? 1600 : width, shot ? 900 : height, SDL_WINDOW_RESIZABLE | (shot ? SDL_WINDOW_HIDDEN : 0));
    if (!window_) return false;
    device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!device_ || !SDL_ClaimWindowForGPUDevice(device_, window_)) return false;
    SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    format_ = SDL_GetGPUSwapchainTextureFormat(device_, window_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 0.07f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1, 1, 1, 0.12f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1, 1, 1, 0.16f);
    style.Colors[ImGuiCol_SliderGrab] = ImGui::ColorConvertU32ToFloat4(kGold);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1, 0.82f, 0.4f, 1);
    style.Colors[ImGuiCol_NavCursor] = ImVec4(0, 0, 0, 0);  // the launcher draws its own selection
    ImGui_ImplSDL3_InitForSDLGPU(window_);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);  // every controller, not only the first
    ImGui_ImplSDLGPU3_InitInfo info = {};
    info.Device = device_;
    info.ColorTargetFormat = format_;
    if (!ImGui_ImplSDLGPU3_Init(&info)) return false;
    load_fonts();
    std::srand(static_cast<unsigned>(SDL_GetPerformanceCounter()));   // (which of our backgrounds, the dust and the trails)
    load_art();
    // The first time: Enhanced, as the other ports start (the game alone starts on its own defaults), unless a style
    // key was already set from outside (RecompLauncher's settings).
    const bool style_key_set = std::any_of(std::begin(kStyleValues), std::end(kStyleValues), [](const StyleValue& v) { return settings_has(v.key); });
    if (!settings_has("launcher_style_custom") && !style_key_set) apply_style(kStyleEnhanced);
    return true;
}

void Launcher::stop() {
    if (setup_thread_.joinable()) cancel_setup();   // closing the launcher ends the tools and compilers too
    if (device_) SDL_WaitForGPUIdle(device_);
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (device_) {
        if (background_) SDL_ReleaseGPUTexture(device_, background_);
        if (logo_) SDL_ReleaseGPUTexture(device_, logo_);
        if (emblem_) SDL_ReleaseGPUTexture(device_, emblem_);
        if (pad_art_) SDL_ReleaseGPUTexture(device_, pad_art_);
        if (pad_bumpers_) SDL_ReleaseGPUTexture(device_, pad_bumpers_);
        if (window_) SDL_ReleaseWindowFromGPUDevice(device_, window_);
        SDL_DestroyGPUDevice(device_);
    }
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

// Barlow Semi Condensed (SIL Open Font License, third_party/fonts), from the fonts folder beside the program.
void Launcher::load_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    const auto add = [&](const char* file, bool symbols) -> ImFont* {
        const std::string path = (exe_directory_ / "fonts" / file).string();
        ImFont* font = fs::exists(path) ? io.Fonts->AddFontFromFileTTF(path.c_str(), 20.0f) : nullptr;
        if (!font) return io.Fonts->AddFontDefault();
        if (symbols && fs::exists("C:\\Windows\\Fonts\\seguisym.ttf")) {  // arrows and other signs Barlow lacks
            ImFontConfig merge;
            merge.MergeMode = true;
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", 20.0f, &merge);
        }
        return font;
    };
    body_ = add("BarlowSemiCondensed-Medium.ttf", true);
    body_bold_ = add("BarlowSemiCondensed-SemiBold.ttf", true);
    italic_ = add("BarlowSemiCondensed-MediumItalic.ttf", false);
    heading_ = add("BarlowSemiCondensed-ExtraBoldItalic.ttf", false);
    black_ = add("BarlowSemiCondensed-BlackItalic.ttf", false);
    io.FontDefault = body_;
}

SDL_GPUTexture* Launcher::make_texture(const unsigned char* rgba, int width, int height) {
    SDL_GPUTextureCreateInfo info = {};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device_, &info);
    if (!texture) return nullptr;
    const Uint32 size = static_cast<Uint32>(width * height * 4);
    SDL_GPUTransferBufferCreateInfo buffer_info = {SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size, 0};
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device_, &buffer_info);
    std::memcpy(SDL_MapGPUTransferBuffer(device_, buffer, false), rgba, size);
    SDL_UnmapGPUTransferBuffer(device_, buffer);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureTransferInfo source = {};
    source.transfer_buffer = buffer;
    SDL_GPUTextureRegion region = {};
    region.texture = texture;
    region.w = info.width;
    region.h = info.height;
    region.d = 1;
    SDL_UploadToGPUTexture(pass, &source, &region, false);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device_, buffer);
    return texture;
}

// The background and the logo, from the player's own copy: sce_sys/pic0.png (the game's picture, blurred here: made
// small and averaged, the GPU's filter does the rest) and sce_sys/livearea/contents/startup.png (its logo). Until
// there is a copy, one of abyssport's own pictures (art/bg_*.jpg), chosen at random. The
// emblem beside the port's name is always ours.
void Launcher::load_art() {
    int w = 0, h = 0, n = 0;
    if (!emblem_) {
        if (unsigned char* emblem = stbi_load((exe_directory_ / "art" / "emblem.png").string().c_str(), &w, &h, &n, 4)) {
            emblem_ = make_texture(emblem, w, h);
            stbi_image_free(emblem);
        }
        if (unsigned char* art = stbi_load((exe_directory_ / "art" / "pad.png").string().c_str(), &w, &h, &n, 4)) {
            pad_art_ = make_texture(art, w, h);
            stbi_image_free(art);
        }
        if (unsigned char* art = stbi_load((exe_directory_ / "art" / "pad_bumpers.png").string().c_str(), &w, &h, &n, 4)) {
            pad_bumpers_ = make_texture(art, w, h);
            stbi_image_free(art);
        }
    }
    const auto own_background = [&] {
        static const char* const kOwn[] = {"bg_temple.jpg", "bg_chasm.jpg", "bg_cave.jpg", "bg_ruins.jpg"};
        if (unsigned char* picture = stbi_load((exe_directory_ / "art" / kOwn[std::rand() % 4]).string().c_str(), &w, &h, &n, 4)) {
            background_ = make_texture(picture, w, h);
            background_width_ = w;
            background_height_ = h;
            stbi_image_free(picture);
        }
    };
    if (!game_ready_) {
        own_background();
        return;
    }
    const fs::path sys = game_directory_ / "fs" / "app0" / "sce_sys";
    if (unsigned char* picture = stbi_load((sys / "pic0.png").string().c_str(), &w, &h, &n, 4)) {
        if (background_) SDL_ReleaseGPUTexture(device_, background_);
        background_is_game_ = true;
        const int sw = 96, sh = std::max(1, 96 * h / std::max(1, w));
        std::vector<float> small(static_cast<size_t>(sw * sh * 3), 0.0f), pass(small.size());
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < sw; ++x) {  // the box of source pixels this one covers
                const int x0 = x * w / sw, x1 = std::max(x0 + 1, (x + 1) * w / sw), y0 = y * h / sh, y1 = std::max(y0 + 1, (y + 1) * h / sh);
                float sum[3] = {};
                for (int v = y0; v < y1; ++v) {
                    for (int u = x0; u < x1; ++u) {
                        for (int c = 0; c < 3; ++c) sum[c] += picture[(v * w + u) * 4 + c];
                    }
                }
                for (int c = 0; c < 3; ++c) small[(y * sw + x) * 3 + c] = sum[c] / static_cast<float>((x1 - x0) * (y1 - y0));
            }
        }
        for (int round = 0; round < 3; ++round) {  // three box blurs: close to a gaussian
            for (int axis = 0; axis < 2; ++axis) {
                for (int y = 0; y < sh; ++y) {
                    for (int x = 0; x < sw; ++x) {
                        for (int c = 0; c < 3; ++c) {
                            float sum = 0;
                            for (int d = -2; d <= 2; ++d) {
                                const int u = std::clamp(axis ? x : x + d, 0, sw - 1), v = std::clamp(axis ? y + d : y, 0, sh - 1);
                                sum += small[(v * sw + u) * 3 + c];
                            }
                            pass[(y * sw + x) * 3 + c] = sum / 5;
                        }
                    }
                }
                small.swap(pass);
            }
        }
        std::vector<unsigned char> rgba(static_cast<size_t>(sw * sh * 4));
        for (int i = 0; i < sw * sh; ++i) {
            for (int c = 0; c < 3; ++c) rgba[i * 4 + c] = static_cast<unsigned char>(std::clamp(small[i * 3 + c], 0.0f, 255.0f));
            rgba[i * 4 + 3] = 255;
        }
        background_ = make_texture(rgba.data(), sw, sh);
        background_width_ = w;
        background_height_ = h;
        stbi_image_free(picture);
    }
    if (unsigned char* logo = stbi_load((sys / "livearea" / "contents" / "startup.png").string().c_str(), &w, &h, &n, 4)) {
        logo_ = make_texture(logo, w, h);
        logo_width_ = w;
        logo_height_ = h;
        stbi_image_free(logo);
    }
    if (!background_) own_background();  // a copy without its picture: one of ours, as before there is a copy
}

// Where the game is: ABYSS_GAME_DIR, the build tree's game/library (the launcher runs from build/<preset>), or the
// player's library in %LOCALAPPDATA%/abyssport. Ready: its memory image is there, made by tools/make_game.py.
void Launcher::find_game() {
    std::vector<fs::path> candidates;
    if (const char* dir = std::getenv("ABYSS_GAME_DIR")) candidates.emplace_back(dir);
    const fs::path relative = fs::path("library") / "decrypted" / "PCSA00029" / "recomp";
    candidates.push_back(exe_directory_ / ".." / ".." / "game" / relative);
    candidates.push_back(exe_directory_ / "game" / relative);
    if (const char* local = std::getenv("LOCALAPPDATA")) candidates.push_back(fs::path(local) / "abyssport" / relative);
    std::error_code error;
    for (const fs::path& candidate : candidates) {
        if (fs::exists(candidate / "game.img", error)) {
            game_directory_ = fs::weakly_canonical(candidate, error);
            game_ready_ = true;
            break;
        }
    }
    if (game_ready_) {
        if (FILE* file = std::fopen((game_directory_ / ".." / "version").string().c_str(), "r")) {
            char line[32] = {};
            if (std::fgets(line, sizeof(line), file)) game_version_ = line;
            while (!game_version_.empty() && std::isspace(static_cast<unsigned char>(game_version_.back()))) game_version_.pop_back();
            std::fclose(file);
        }
    }
    // The program: beside the launcher in the build tree; in a release, beside the player's library, where the first
    // run links it (<base>/library/decrypted/PCSA00029/recomp -> <base>/abyss.exe).
    program_ = exe_directory_ / "abyss.exe";
    if (!fs::exists(program_, error) && game_ready_) program_ = game_directory_.parent_path().parent_path().parent_path().parent_path() / "abyss.exe";
    program_ready_ = fs::exists(program_, error);
    tools_ready_ = fs::exists(exe_directory_ / "python" / "python.exe", error) && fs::exists(exe_directory_ / "tools" / "make_game.py", error) &&
                   fs::exists(exe_directory_ / "runtime" / "lib" / "libabyssrt.a", error);
}

// --- the first run: the game made from the player's copy ---

// Where the library and the program go: beside the launcher when its folder can be written to, else the player's
// %LOCALAPPDATA%\abyssport (both are where find_game looks).
fs::path Launcher::setup_base() const {
    const fs::path beside = exe_directory_ / "game";
    std::error_code error;
    fs::create_directories(beside, error);
    const fs::path probe = beside / ".write-test";
    if (FILE* file = _wfopen(probe.c_str(), L"wb")) {
        std::fclose(file);
        fs::remove(probe, error);
        return beside;
    }
    const wchar_t* local = _wgetenv(L"LOCALAPPDATA");
    return local ? fs::path(local) / "abyssport" : beside;
}

// One tool, its output read line by line: "[k/5] ..." from make_game.py, "  k/N" from build_game.py; the last lines
// kept for the error. False when it failed or was cancelled.
bool Launcher::run_tool(const std::wstring& command, bool making) {
    SECURITY_ATTRIBUTES inherit = {sizeof(inherit), nullptr, TRUE};
    HANDLE read = nullptr, write = nullptr;
    if (!CreatePipe(&read, &write, &inherit, 0)) return false;
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = startup.hStdError = write;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process = {};
    std::wstring line_buffer = command;
    const bool started = CreateProcessW(nullptr, line_buffer.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                                        exe_directory_.c_str(), &startup, &process);
    CloseHandle(write);
    if (!started) {
        CloseHandle(read);
        std::lock_guard<std::mutex> hold(setup_lock_);
        setup_error_ = "Python did not start: " + (exe_directory_ / "python" / "python.exe").string();
        return false;
    }
    AssignProcessToJobObject(setup_job_, process.hProcess);
    ResumeThread(process.hThread);
    std::string pending;
    char chunk[4096];
    DWORD got = 0;
    while (ReadFile(read, chunk, sizeof(chunk), &got, nullptr) && got) {
        pending.append(chunk, got);
        size_t end;
        while ((end = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, end);
            pending.erase(0, end + 1);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            std::lock_guard<std::mutex> hold(setup_lock_);
            int k = 0, n = 0;
            if (making && std::sscanf(line.c_str(), "[%d/%d]", &k, &n) == 2 && n > 0) {
                setup_step_ = std::clamp(k - 1, 0, 4);
                setup_progress_ = 0.75f * (k - 1) / n;
            } else if (!making && std::sscanf(line.c_str(), " %d/%d", &k, &n) == 2 && n > 0) {
                setup_step_ = 5;
                setup_progress_ = (game_ready_ ? 0.0f : 0.75f) + (game_ready_ ? 0.95f : 0.22f) * k / n;
            } else if (!making && line == "linking") {
                setup_step_ = 6;
            }
            if (line.rfind("make_game:", 0) == 0 || line.rfind("build_game:", 0) == 0) setup_error_ = line.substr(line.find(':') + 2);
            setup_tail_ += line + "\n";
            if (setup_tail_.size() > 1500) setup_tail_.erase(0, setup_tail_.size() - 1500);
        }
    }
    CloseHandle(read);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code == 0;
}

// Prepare: make_game.py from the copy and the firmware (skipped when the game is already made), then build_game.py,
// on a worker thread. The launcher looks for the game again when it is done.
void Launcher::start_setup() {
    const Texts& t = T();
    const fs::path base = setup_base();
    const fs::path library = base / "library";
    // the game already made: its own library, wherever it is
    const fs::path recomp = game_ready_ ? game_directory_ : library / "decrypted" / "PCSA00029" / "recomp";
    const int need_gb = game_ready_ ? 1 : 8;   // the library is 3.6 GB, the firmware and the game's copy decrypted on the way
    std::error_code error;
    fs::create_directories(base, error);
    const fs::space_info space = fs::space(base, error);
    {
        std::lock_guard<std::mutex> hold(setup_lock_);
        setup_error_.clear();
        setup_tail_.clear();
        setup_step_ = game_ready_ ? 5 : 0;
        setup_progress_ = 0;
        setup_done_ = setup_cancelled_ = false;
        if (!error && space.available < static_cast<std::uintmax_t>(need_gb) << 30) {
            char message[512];
            std::snprintf(message, sizeof(message), t.space, need_gb, base.string().c_str());
            setup_error_ = message;
            return;
        }
        setup_running_ = true;
    }
    setup_job_ = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(setup_job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

    auto quote = [](const fs::path& p) { return L"\"" + p.wstring() + L"\""; };
    auto utf8_path = [](const std::string& s) { return fs::u8path(s); };
    const fs::path python = exe_directory_ / "python" / "python.exe";
    std::vector<std::pair<std::wstring, bool>> commands;
    if (!game_ready_) {
        commands.push_back({quote(python) + L" -u " + quote(exe_directory_ / "tools" / "make_game.py") + L" " + quote(utf8_path(chosen_copy_)) + L" " +
                                quote(utf8_path(chosen_pup_)) + L" --library " + quote(library), true});
    }
    const fs::path program = recomp.parent_path().parent_path().parent_path().parent_path() / "abyss.exe";
    commands.push_back({quote(python) + L" -u " + quote(exe_directory_ / "tools" / "build_game.py") + L" " + quote(recomp) + L" " +
                            quote(exe_directory_ / "runtime") + L" " + quote(exe_directory_ / "toolchain" / "llvm-mingw") + L" " + quote(program), false});
    SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");
    SetEnvironmentVariableW(L"PYTHONUTF8", L"1");

    if (setup_thread_.joinable()) setup_thread_.join();
    setup_thread_ = std::thread([this, commands]() {
        bool ok = true;
        for (const auto& [command, making] : commands) {
            if (!(ok = run_tool(command, making))) break;
        }
        std::lock_guard<std::mutex> hold(setup_lock_);
        if (!ok && setup_error_.empty() && !setup_cancelled_) {   // no message of the tools': their last line
            std::string tail = setup_tail_;
            while (!tail.empty() && tail.back() == '\n') tail.pop_back();
            setup_error_ = tail.substr(tail.rfind('\n') == std::string::npos ? 0 : tail.rfind('\n') + 1);
            if (setup_error_.empty()) setup_error_ = "?";
        }
        if (ok) setup_progress_ = 1;
        setup_running_ = false;
        setup_done_ = true;
    });
}

void Launcher::cancel_setup() {
    {
        std::lock_guard<std::mutex> hold(setup_lock_);
        setup_cancelled_ = true;
    }
    if (setup_job_) TerminateJobObject(setup_job_, 1);
    if (setup_thread_.joinable()) setup_thread_.join();
    if (setup_job_) CloseHandle(setup_job_);
    setup_job_ = nullptr;
}

// The rows of the first run, on the Play page: the copy and the firmware file, chosen in Windows' own dialogs; the
// steps with a bar while it runs; what went wrong.
void Launcher::setup_rows() {
    const Texts& t = T();
    std::unique_lock<std::mutex> hold(setup_lock_);
    if (setup_done_) {   // the worker has finished: the game looked for again
        setup_done_ = false;
        hold.unlock();
        if (setup_thread_.joinable()) setup_thread_.join();
        if (setup_job_) CloseHandle(setup_job_);
        setup_job_ = nullptr;
        find_game();
        if (game_ready_) settings_load((game_directory_ / "settings.ini").string().c_str());
        if (game_ready_ && !background_is_game_) load_art();   // the game's own artwork, now that there is a copy of it
        hold.lock();
    }
    const bool running = setup_running_;
    if (!game_ready_ && !running) {
        // the dialogs answer on SDL's thread
        auto picked_copy = [](void* self, const char* const* list, int) {
            if (!list || !list[0]) return;
            std::lock_guard<std::mutex> hold(static_cast<Launcher*>(self)->setup_lock_);
            static_cast<Launcher*>(self)->chosen_copy_ = list[0];
        };
        auto picked_pup = [](void* self, const char* const* list, int) {
            if (!list || !list[0]) return;
            std::lock_guard<std::mutex> hold(static_cast<Launcher*>(self)->setup_lock_);
            static_cast<Launcher*>(self)->chosen_pup_ = list[0];
        };
        const std::string copy = chosen_copy_, pup = chosen_pup_;
        hold.unlock();
        if (status(!copy.empty(), t.copy, copy.empty() ? t.copy_none : copy.c_str(), t.choose, "##copy")) {
            SDL_ShowOpenFolderDialog(picked_copy, this, window_, copy.empty() ? nullptr : copy.c_str(), false);
        }
        if (status(!pup.empty(), t.pup, pup.empty() ? t.pup_none : pup.c_str(), t.choose, "##pup")) {
            static const SDL_DialogFileFilter filter = {"PUP", "PUP;pup"};
            SDL_ShowOpenFileDialog(picked_pup, this, window_, &filter, 1, pup.empty() ? nullptr : pup.c_str(), false);
        }
        hold.lock();
    }
    if (running) {
        const float now = static_cast<float>(ImGui::GetTime());
        const ImVec2 a(origin_.x, origin_.y + cursor_);
        text(body_bold_, px(21), a, kInk, t.steps[setup_step_]);
        const float bar_y = a.y + px(34), width = panel_width_ - px(140);
        list_->AddRectFilled(ImVec2(a.x, bar_y), ImVec2(a.x + width, bar_y + px(8)), fade(IM_COL32(255, 255, 255, 30)));
        list_->AddRectFilled(ImVec2(a.x, bar_y), ImVec2(a.x + width * std::clamp(setup_progress_, 0.02f, 1.0f), bar_y + px(8)), fade(kGold));
        const float sweep = std::fmod(now, 1.6f) / 1.6f;   // it moves even when a step takes minutes
        list_->AddRectFilled(ImVec2(a.x + width * setup_progress_ * sweep, bar_y), ImVec2(a.x + width * setup_progress_ * sweep + px(30), bar_y + px(8)),
                             fade(IM_COL32(255, 230, 160, 120)));
        hold.unlock();
        if (small_button(t.cancel, ImVec2(a.x + width + px(20), a.y + px(18)), "##cancel")) cancel_setup();
        cursor_ += px(64);
        return;
    }
    const std::string failure = setup_error_, tail = setup_tail_;
    const bool can_start = game_ready_ || (!chosen_copy_.empty() && !chosen_pup_.empty());
    hold.unlock();
    if (!failure.empty()) {
        status(false, t.failed, failure.c_str(), nullptr, "##failed", true);
        if (!tail.empty()) {
            const char* last = tail.c_str() + tail.size();   // the last four lines, small
            for (int lines = 0; last > tail.c_str() && lines < 5; ) if (*--last == '\n') ++lines;
            text(body_, px(14), ImVec2(origin_.x + px(46), origin_.y + cursor_), kDim, last, origin_.x + panel_width_);
            cursor_ += px(80);
        }
    }
    lead(game_ready_ ? t.compile_only : t.duration);
    if (can_start && big_button(t.prepare, "##prepare", true)) start_setup();
}

// The game, as play.cmd starts it: in its own folder, told which libraries to load and where its shader recipes are.
// The launcher's window is hidden meanwhile and comes back when the game ends.
void Launcher::start_game() {
    settings_save();
    SetEnvironmentVariableA("RECOMP_MODULES", "libc.img,libgxm_es4.img,libgpu_es4.img,libfios2.img,libscemp4.img");
    SetEnvironmentVariableA("RECOMP_TITLE", "PCSA00029");
    SetEnvironmentVariableA("RECOMP_GAME_NAME", "Uncharted: Golden Abyss");
    SetEnvironmentVariableA("RECOMP_FS", "fs");
    SetEnvironmentVariableA("RECOMP_LOG", "0");
    SetEnvironmentVariableA("ABYSS_LAUNCHER", "1");  // the in-game menu's Quit reads "Back to the launcher" (overlay.cpp)
    std::error_code error;
    for (const fs::path& recipes : {exe_directory_ / "shaders.txt", exe_directory_ / ".." / ".." / "res" / "shaders.txt"}) {
        if (fs::exists(recipes, error)) {
            SetEnvironmentVariableA("RECOMP_SHADER_RECIPES_SHIPPED", fs::weakly_canonical(recipes, error).string().c_str());
            break;
        }
    }
    // what the game prints goes to abyss.log in its folder, for a bug report (the last run's only)
    SECURITY_ATTRIBUTES inherit = {sizeof(inherit), nullptr, TRUE};
    HANDLE log = CreateFileW((game_directory_ / "abyss.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    std::wstring command = L"\"" + program_.wstring() + L"\" game.img 81009ffc 100000000000";
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    if (log != INVALID_HANDLE_VALUE) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = startup.hStdError = log;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    PROCESS_INFORMATION process = {};
    const bool started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, log != INVALID_HANDLE_VALUE, 0, nullptr, game_directory_.c_str(), &startup, &process);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (!started) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "abyssport", "abyss.exe did not start.", window_);
        return;
    }
    SDL_HideWindow(window_);
    WaitForSingleObject(process.hProcess, INFINITE);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    settings_load((game_directory_ / "settings.ini").string().c_str());  // what the in-game menu changed
    start_time_ = page_since_ = static_cast<float>(ImGui::GetTime());
}

// --- drawing helpers ---

// A parallelogram leaning right like an italic menu, coloured left to right.
void Launcher::skewed_rect(ImDrawList* list, ImVec2 a, ImVec2 b, ImU32 left, ImU32 right, float skew_ratio) {
    const float skew = (b.y - a.y) * skew_ratio;
    const ImVec2 uv = ImGui::GetDrawListSharedData()->TexUvWhitePixel;
    list->PrimReserve(6, 4);
    const ImDrawIdx index = static_cast<ImDrawIdx>(list->_VtxCurrentIdx);
    list->PrimWriteIdx(index);
    list->PrimWriteIdx(static_cast<ImDrawIdx>(index + 1));
    list->PrimWriteIdx(static_cast<ImDrawIdx>(index + 2));
    list->PrimWriteIdx(index);
    list->PrimWriteIdx(static_cast<ImDrawIdx>(index + 2));
    list->PrimWriteIdx(static_cast<ImDrawIdx>(index + 3));
    list->PrimWriteVtx(ImVec2(a.x + skew, a.y), uv, fade(left));
    list->PrimWriteVtx(ImVec2(b.x + skew, a.y), uv, fade(right));
    list->PrimWriteVtx(ImVec2(b.x, b.y), uv, fade(right));
    list->PrimWriteVtx(ImVec2(a.x, b.y), uv, fade(left));
}

float Launcher::anim(ImGuiID id, bool on, float speed) {
    float& value = anims_[id];
    value += ((on ? 1.0f : 0.0f) - value) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return value;
}

// The console's buttons, drawn with lines, circles and rounded boxes (no textures): the four face buttons in their
// colours, L and R, the left stick, the d-pad with one arm lit, Start and Select. Returns the width used.
float Launcher::glyph(const char* name, ImVec2 at, float h) {
    const std::string n = name;
    const ImVec2 c(at.x + h / 2, at.y + h / 2);
    const float r = h / 2, line = std::max(1.5f, h / 11);
    const auto disc = [&] {
        list_->AddCircleFilled(c, r, fade(IM_COL32(24, 24, 28, 255)));
        list_->AddCircle(c, r, fade(IM_COL32(255, 255, 255, 70)), 0, px(1));
    };
    if (n == "X") {
        disc();
        const float d = r * 0.42f;
        list_->AddLine(ImVec2(c.x - d, c.y - d), ImVec2(c.x + d, c.y + d), fade(IM_COL32(124, 178, 232, 255)), line);
        list_->AddLine(ImVec2(c.x - d, c.y + d), ImVec2(c.x + d, c.y - d), fade(IM_COL32(124, 178, 232, 255)), line);
    } else if (n == "O") {
        disc();
        list_->AddCircle(c, r * 0.48f, fade(IM_COL32(255, 102, 102, 255)), 0, line);
    } else if (n == "S") {
        disc();
        const float d = r * 0.42f;
        list_->AddRect(ImVec2(c.x - d, c.y - d), ImVec2(c.x + d, c.y + d), fade(IM_COL32(255, 105, 248, 255)), 0, 0, line);
    } else if (n == "T") {
        disc();
        const float d = r * 0.5f;
        list_->AddTriangle(ImVec2(c.x, c.y - d), ImVec2(c.x + d, c.y + d * 0.75f), ImVec2(c.x - d, c.y + d * 0.75f), fade(IM_COL32(64, 226, 160, 255)), line);
    } else if (n == "L" || n == "R" || n == "ST" || n == "SE") {
        const char* label = n == "ST" ? "START" : n == "SE" ? "SELECT" : name;
        const float size = n.size() == 1 ? h * 0.62f : h * 0.42f;
        const ImVec2 text_size = measure(body_bold_, size, label);
        const float width = std::max(h * 1.5f, text_size.x + h * 0.7f);
        list_->AddRectFilled(at, ImVec2(at.x + width, at.y + h), fade(IM_COL32(232, 232, 236, 255)), h * 0.3f);
        text(body_bold_, size, ImVec2(at.x + (width - text_size.x) / 2, at.y + (h - text_size.y) / 2), IM_COL32(16, 16, 18, 255), label);
        return width;
    } else if (n == "LS") {
        list_->AddCircle(c, r * 0.95f, fade(kChrome), 0, line);
        list_->AddCircleFilled(c, r * 0.5f, fade(kChrome));
    } else if (n[0] == 'D') {
        const float arm = r * 0.34f;
        const ImU32 grey = fade(IM_COL32(232, 232, 236, 255));
        list_->AddRectFilled(ImVec2(c.x - arm, c.y - r), ImVec2(c.x + arm, c.y + r), grey, arm * 0.4f);
        list_->AddRectFilled(ImVec2(c.x - r, c.y - arm), ImVec2(c.x + r, c.y + arm), grey, arm * 0.4f);
        const ImVec2 dir = n == "DU" ? ImVec2(0, -1) : n == "DD" ? ImVec2(0, 1) : n == "DL" ? ImVec2(-1, 0) : ImVec2(1, 0);
        const ImVec2 lit(c.x + dir.x * r * 0.62f, c.y + dir.y * r * 0.62f);
        list_->AddRectFilled(ImVec2(lit.x - arm, lit.y - arm), ImVec2(lit.x + arm, lit.y + arm), fade(kGold), arm * 0.4f);
    }
    return h;
}

void Launcher::title(const char* str) {
    enter_item();
    text(black_, px(48), ImVec2(origin_.x, origin_.y + cursor_), kInk, str);
    cursor_ += px(56);
}

void Launcher::lead(const char* str) {
    enter_item();
    const ImVec2 size = measure(body_, px(21), str, panel_width_);
    text(body_, px(21), ImVec2(origin_.x, origin_.y + cursor_), kDim, str, panel_width_);
    cursor_ += size.y + px(20);
}

bool Launcher::small_button(const char* label, ImVec2 at, const char* id, ImVec2* size_out) {
    const ImVec2 size = measure(body_bold_, px(18), label);
    const ImVec2 box(size.x + px(28), size.y + px(12));
    ImGui::SetCursorScreenPos(at);
    claim_focus();
    const bool pressed = ImGui::InvisibleButton(id, box, kNavigable);
    const float hot = anim(ImGui::GetItemID(), ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride) || nav_selected());
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const bool selected = nav_selected();
    list_->AddRectFilled(at, ImVec2(at.x + box.x, at.y + box.y), selected ? fade(kGoldDark) : fade(kGold, hot * 0.25f));
    list_->AddRect(at, ImVec2(at.x + box.x, at.y + box.y), fade(mix(IM_COL32(255, 255, 255, 60), kGold, hot)), 0, 0, px(1.5f));
    focus_frame(ImVec2(origin_.x - px(24), at.y - px(7)), ImVec2(origin_.x + panel_width_ + px(14), at.y + box.y + px(7)));
    text(body_bold_, px(18), ImVec2(at.x + px(14), at.y + px(6)), selected ? IM_COL32_WHITE : mix(kInk, IM_COL32(255, 220, 150, 255), hot), label);
    if (size_out) *size_out = box;
    return pressed;
}

bool Launcher::status(bool good, const char* heading, const char* detail, const char* button_label, const char* id, bool pulse) {
    enter_item();
    const float height = px(64);
    const ImVec2 a(origin_.x, origin_.y + cursor_), b(origin_.x + panel_width_, a.y + height);
    const ImU32 accent = good ? kGreen : kAmber;
    list_->AddRectFilled(a, b, fade(pulse ? IM_COL32(255, 178, 30, 22) : IM_COL32(255, 255, 255, 10)));
    const ImVec2 dot(a.x + px(26), a.y + height / 2);
    list_->AddCircleFilled(dot, px(6), fade(accent));
    list_->AddCircleFilled(dot, px(12), fade(accent, 0.18f));
    if (pulse) {
        const float p = std::fmod(static_cast<float>(ImGui::GetTime()), 1.8f) / 1.8f;
        list_->AddCircle(dot, px(12) + px(14) * p, fade(accent, 0.6f * (1 - p)), 0, px(2));
    }
    const float button_x = button_label ? b.x - measure(body_bold_, px(18), button_label).x - px(28) - px(14) : b.x;
    list_->PushClipRect(a, ImVec2(button_x - px(12), b.y), true);  // texts stop before the button
    text(body_bold_, px(21), ImVec2(a.x + px(46), a.y + px(9)), kInk, heading);
    text(body_, px(17), ImVec2(a.x + px(46), a.y + px(35)), kDim, detail);
    list_->PopClipRect();
    const bool keep_focus_request = focus_panel_ && status_keeps_focus_;
    if (keep_focus_request) focus_panel_ = false;
    const bool pressed = button_label && small_button(button_label, ImVec2(button_x, a.y + px(14)), id);
    if (button_label) focus_frame(ImVec2(a.x - px(24), a.y), ImVec2(b.x + px(14), b.y));
    if (keep_focus_request) focus_panel_ = true;
    cursor_ += height + px(10);
    return pressed;
}

bool Launcher::big_button(const char* label, const char* id, bool pulse) {
    enter_item();
    const ImVec2 size = measure(black_, px(40), label);
    const ImVec2 a(origin_.x, origin_.y + cursor_ + px(10)), b(a.x + size.x + px(110), a.y + px(64));
    ImGui::SetCursorScreenPos(a);
    claim_focus();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(b.x - a.x + px(14), b.y - a.y), kNavigable);
    const float hot = anim(ImGui::GetItemID(), ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride) || nav_selected());
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    focus_frame(ImVec2(origin_.x - px(24), a.y - px(8)), ImVec2(origin_.x + panel_width_ + px(14), b.y + px(8)));
    if (pulse) {
        const float p = std::fmod(static_cast<float>(ImGui::GetTime()), 2.2f) / 2.2f / 0.7f;
        if (p < 1) {
            const float grow = px(18) * p;
            skewed_rect(list_, ImVec2(a.x - grow, a.y - grow), ImVec2(b.x + grow, b.y + grow), fade(kGold, 0.45f * (1 - p)), fade(kGold, 0.45f * (1 - p)));
        }
    }
    skewed_rect(list_, a, b, mix(kGold, IM_COL32(255, 206, 96, 255), hot * 0.6f), mix(kGoldDark, kGold, hot * 0.5f));
    if (pulse) {  // ready: a light runs round the button's edge, a lap every 2.4 s
        const float skew = (b.y - a.y) * 0.21f;
        const ImVec2 corners[] = {ImVec2(a.x + skew, a.y), ImVec2(b.x + skew, a.y), b, ImVec2(a.x, b.y)};  // skewed_rect's
        float lengths[4], perimeter = 0;
        for (int i = 0; i < 4; ++i) {
            const float dx = corners[(i + 1) % 4].x - corners[i].x, dy = corners[(i + 1) % 4].y - corners[i].y;
            perimeter += lengths[i] = std::sqrt(dx * dx + dy * dy);
        }
        const float head = std::fmod(static_cast<float>(ImGui::GetTime()) / 2.4f, 1.0f) * perimeter, tail = px(160);
        constexpr int kSteps = 24;
        for (int s = 0; s < kSteps; ++s) {  // the tail fades out behind the head
            const float d0 = head - tail * static_cast<float>(s) / kSteps, d1 = head - tail * static_cast<float>(s + 1) / kSteps;
            const auto point = [&](float d) {
                d = std::fmod(d + perimeter, perimeter);
                int i = 0;
                while (i < 3 && d > lengths[i]) d -= lengths[i++];
                return ImLerp(corners[i], corners[(i + 1) % 4], d / lengths[i]);
            };
            const float light = 1 - static_cast<float>(s) / kSteps;
            list_->AddLine(point(d0), point(d1), fade(mix(IM_COL32(255, 238, 190, 255), IM_COL32_WHITE, light), light), px(3));
        }
    }
    text(black_, px(40), ImVec2(a.x + px(55) + px(7), a.y + px(10)), IM_COL32(18, 14, 6, 255), label);
    cursor_ += px(90);
    return pressed;
}

bool Launcher::tabs(const char* const* labels, int count, int* current) {
    enter_item();
    float x = origin_.x;
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        const ImVec2 size = measure(heading_, px(22), labels[i]);
        ImGui::SetCursorScreenPos(ImVec2(x, origin_.y + cursor_));
        ImGui::PushID(i);
        if (i == *current) claim_focus();
        if (ImGui::InvisibleButton("##tab", ImVec2(size.x, size.y + px(10)), kNavigable)) {
            *current = i;
            changed = true;
        }
        const float hot = anim(ImGui::GetItemID(), ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride) || nav_selected());
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        focus_frame(ImVec2(ImGui::GetItemRectMin().x - px(16), ImGui::GetItemRectMin().y - px(4)), ImGui::GetItemRectMax());
        ImGui::PopID();
        const bool on = *current == i;
        text(heading_, px(22), ImVec2(x, origin_.y + cursor_), on ? kInk : mix(kDim, kInk, hot), labels[i]);
        if (on) list_->AddRectFilled(ImVec2(x, origin_.y + cursor_ + size.y + px(4)), ImVec2(x + size.x, origin_.y + cursor_ + size.y + px(8)), fade(kGold));
        x += size.x + px(28);
    }
    // Q and E, LB and RB: the previous and next tab.
    if (rebinding_ < 0) {
        if (key_pressed({ImGuiKey_GamepadL1, ImGuiKey_Q, ImGuiKey_PageUp}) && *current > 0) { --*current; changed = true; }
        if (key_pressed({ImGuiKey_GamepadR1, ImGuiKey_E, ImGuiKey_PageDown}) && *current < count - 1) { ++*current; changed = true; }
    }
    cursor_ += px(48);
    return changed;
}

void Launcher::row(const char* heading, const char* detail, float control) {
    enter_item();
    text(body_bold_, px(20), ImVec2(origin_.x, origin_.y + cursor_ + px(4)), kInk, heading);
    if (!detail) return;
    // Left of the control (`control` wide), or under a wide one across the whole row (squeezed beside it, it took four
    // lines): a longer detail wraps, and the row grows by the extra lines.
    const bool under = control >= 330;
    const float wrap = under ? panel_width_ : panel_width_ - px(control + 20), top = px(under ? 60 : 29);
    text(body_, px(17), ImVec2(origin_.x, origin_.y + cursor_ + top), kDim, detail, wrap);
    row_extra_ = std::max(0.0f, measure(body_, px(17), detail, wrap).y - px(22) + top - px(29));
}

bool Launcher::segmented(const char* id, const char* const* labels, int count, int* current, const char* const* details, float wide) {
    const float width = px(wide > 0 ? wide : 270), height = px(details ? 50 : 36);
    const ImVec2 a(origin_.x + panel_width_ - width, origin_.y + cursor_ + px(6));
    skewed_rect(list_, a, ImVec2(a.x + width, a.y + height), IM_COL32(255, 255, 255, 18), IM_COL32(255, 255, 255, 18));
    const float cell = width / static_cast<float>(count);
    bool changed = false;
    ImGui::PushID(id);
    const float row_top = origin_.y + cursor_;
    ImGui::SetCursorScreenPos(a);
    claim_focus();
    const bool pressed = ImGui::InvisibleButton("##value", ImVec2(width, height), kNavigable);
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride);  // the mouse, not the selection
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const int hovered_cell = hovered ? std::clamp(static_cast<int>((ImGui::GetIO().MousePos.x - a.x) / cell), 0, count - 1) : -1;
    int next = *current;
    if (pressed && hovered_cell >= 0) next = hovered_cell;  // the mouse; Enter changes nothing, left and right do
    next = std::clamp(next + value_step(), 0, count - 1);
    if (next != *current) {
        *current = next;
        changed = true;
        value_moved_frame_ = ImGui::GetFrameCount();
    }
    focus_frame(ImVec2(origin_.x - px(24), row_top), ImVec2(origin_.x + panel_width_ + px(14), row_top + px(details ? 66 : 52) + row_extra_));
    for (int i = 0; i < count; ++i) {
        const ImVec2 c(a.x + cell * static_cast<float>(i), a.y);
        ImGui::PushID(i);
        const float hot = anim(ImGui::GetID("##hot"), hovered_cell == i);
        const float on = anim(ImGui::GetID("##on"), *current == i, 16);
        ImGui::PopID();
        if (on > 0.01f || hot > 0.01f) {
            skewed_rect(list_, c, ImVec2(c.x + cell, c.y + height), fade(mix(IM_COL32(255, 255, 255, 30), kGold, on), std::max(on, hot)),
                        fade(mix(IM_COL32(255, 255, 255, 30), kGoldDark, on), std::max(on, hot)));
        }
        const ImVec2 size = measure(body_bold_, px(18), labels[i]);
        const ImU32 ink = mix(kDim, mix(IM_COL32(255, 255, 255, 255), IM_COL32(18, 14, 6, 255), on), std::max(on, hot));
        if (details && details[i]) {  // a second, smaller line under the label (the lines of a resolution)
            const ImVec2 detail = measure(body_, px(14), details[i]);
            const float top = c.y + (height - size.y - detail.y) / 2;
            text(body_bold_, px(18), ImVec2(c.x + (cell - size.x) / 2 + px(3), top), ink, labels[i]);
            text(body_, px(14), ImVec2(c.x + (cell - detail.x) / 2 + px(2), top + size.y), fade(ink, 0.75f), details[i]);
        } else {
            text(body_bold_, px(18), ImVec2(c.x + (cell - size.x) / 2 + px(3), c.y + (height - size.y) / 2), ink, labels[i]);
        }
    }
    ImGui::PopID();
    cursor_ += px(details ? 72 : 58) + row_extra_;
    row_extra_ = 0;
    list_->AddLine(ImVec2(origin_.x, origin_.y + cursor_ - px(6)), ImVec2(origin_.x + panel_width_, origin_.y + cursor_ - px(6)), fade(IM_COL32(255, 255, 255, 16)));
    return changed;
}

bool Launcher::value_slider(const char* id, float* value, float minimum, float maximum, const char* format,
                            float font_size) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
    const bool changed = ImGui::SliderFloat(id, value, minimum, maximum, format);
    ImGui::PopStyleColor();
    char label[32];
    std::snprintf(label, sizeof(label), format, *value);
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax(), size = measure(body_bold_, font_size, label);
    const float grab = ImGui::GetStyle().GrabMinSize, t = std::clamp((*value - minimum) / (maximum - minimum), 0.0f, 1.0f);
    const float centre = a.x + 2 + grab / 2 + t * (b.x - a.x - 4 - grab);  // ImGui's grab, 2 px in from the frame
    const float x = t < 0.5f ? centre + grab / 2 + px(8) : centre - grab / 2 - px(8) - size.x;
    text(body_bold_, font_size, ImVec2(x, a.y + (b.y - a.y - size.y) / 2), kInk, label);
    return changed;
}

bool Launcher::slider(const char* id, float* value, float minimum, float maximum, const char* format) {
    const float width = px(270);
    ImGui::SetCursorScreenPos(ImVec2(origin_.x + panel_width_ - width, origin_.y + cursor_ + px(8)));
    ImGui::PushItemWidth(width);
    ImGui::PushFont(body_bold_, px(18));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(px(8), px(5)));
    claim_focus();
    bool changed = value_slider(id, value, minimum, maximum, format, px(18));
    focus_ring();
    changed |= step_slider(value, minimum, maximum);
    focus_frame(ImVec2(origin_.x - px(24), origin_.y + cursor_), ImVec2(origin_.x + panel_width_ + px(14), origin_.y + cursor_ + px(46) + row_extra_));
    ImGui::PopStyleVar();
    ImGui::PopFont();
    ImGui::PopItemWidth();
    cursor_ += px(52) + row_extra_;
    row_extra_ = 0;
    list_->AddLine(ImVec2(origin_.x, origin_.y + cursor_ - px(6)), ImVec2(origin_.x + panel_width_, origin_.y + cursor_ - px(6)), fade(IM_COL32(255, 255, 255, 16)));
    return changed;
}

// A thin gold frame around the last ImGui widget while it is hovered, like the launcher's own buttons, and the row's
// mark while it is selected.
void Launcher::focus_ring() {
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride);
    const float hot = anim(ImGui::GetItemID(), hovered);
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    if (hot >= 0.01f) list_->AddRect(ImVec2(a.x - px(2), a.y - px(2)), ImVec2(b.x + px(2), b.y + px(2)), fade(kGold, hot), 0, 0, px(1.5f));
    focus_frame(a, b);
}

// Left or right on the selected row: -1 or 1; 0 for nothing.
int Launcher::value_step() {
    if (!nav_selected() || rebinding_ >= 0) return 0;
    if (ImGui::GetFrameCount() == entered_page_frame_) return 0;  // the Right that came into the page, not a change
    const auto held = [](ImGuiKey key) { return ImGui::IsKeyDown(key); };
    const bool left = held(ImGuiKey_LeftArrow) || held(ImGuiKey_GamepadDpadLeft);
    if (!left && !held(ImGuiKey_RightArrow) && !held(ImGuiKey_GamepadDpadRight)) return 0;
    ImGui::NavMoveRequestCancel();  // the arrows belong to the value, not to the next widget
    // Held at the far end, left stays on the row: only a new press leaves the page.
    if (left && !ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) && !ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft, false)) {
        value_moved_frame_ = ImGui::GetFrameCount();
    }
    if (menu_key_pressed({ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft})) return -1;
    if (menu_key_pressed({ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight})) return 1;
    return 0;
}

bool Launcher::step_slider(float* value, float minimum, float maximum) {
    if (ImGui::IsItemActive()) return false;  // Enter took it: ImGui's own keys move it
    const int step = value_step();
    if (!step) return false;
    const float next = std::clamp(*value + static_cast<float>(step) * (maximum - minimum) / 20.0f, minimum, maximum);
    if (next == *value) return false;
    *value = next;
    value_moved_frame_ = ImGui::GetFrameCount();
    return true;
}

// --- settings ---

void Launcher::choice(const char* key, const char* def, const char* const* values, const char* const* labels, int count,
                      const char* const* details, float wide) {
    const std::string now = setting(key, def);
    int current = 0;
    for (int i = 0; i < count; ++i) {
        if (now == values[i] || (std::atof(now.c_str()) != 0 && std::atof(now.c_str()) == std::atof(values[i]))) current = i;
    }
    if (segmented(key, labels, count, &current, details, wide)) {
        settings_set_str(key, values[current]);
        settings_save();
    }
}

void Launcher::number(const char* key, float def, float minimum, float maximum, const char* format) {
    float value = static_cast<float>(settings_float(key, def));
    const std::string id = std::string("##") + key;
    if (slider(id.c_str(), &value, minimum, maximum, format)) {
        settings_set_float(key, std::round(value * 100) / 100);
        settings_save();
    }
}

// The internal resolution that fills the screen: the game draws 544 lines.
std::string Launcher::auto_scale() const {
    int lines = 1080;
    SDL_Rect bounds;
    if (const SDL_DisplayID display = SDL_GetDisplayForWindow(window_); display && SDL_GetDisplayBounds(display, &bounds)) lines = bounds.h;
    return std::to_string(std::clamp(static_cast<int>(std::ceil(lines / 544.0 - 0.05)), 1, 6));
}

int Launcher::style() const {
    if (settings_int("launcher_style_custom", 0)) return kStyleCustom;
    bool original = true, enhanced = true;
    for (const StyleValue& value : kStyleValues) {
        const std::string text_now = setting(value.key, value.runtime);
        const double now = std::atof(text_now.c_str());
        const std::string wanted_enhanced = std::strcmp(value.enhanced, "auto") ? value.enhanced : auto_scale();
        const auto same = [&](const std::string& wanted) {
            return text_now == wanted || (std::isdigit(static_cast<unsigned char>(wanted[0])) && std::fabs(now - std::atof(wanted.c_str())) < 0.001);
        };
        original = original && same(value.original);
        enhanced = enhanced && same(wanted_enhanced);
    }
    return original ? kStyleOriginal : enhanced ? kStyleEnhanced : kStyleCustom;
}

void Launcher::apply_style(int chosen) {
    settings_set_int("launcher_style_custom", chosen == kStyleCustom);
    if (chosen != kStyleCustom) {
        for (const StyleValue& value : kStyleValues) {
            const char* wanted = chosen == kStyleOriginal ? value.original : value.enhanced;
            settings_set_str(value.key, std::strcmp(wanted, "auto") ? wanted : auto_scale().c_str());
        }
    }
    settings_save();
}

void Launcher::style_row() {
    const Texts& t = T();
    int current = style();
    row(t.style, t.style_details[current], 400);
    if (segmented("style", t.style_names, 3, &current, nullptr, 400)) apply_style(current);
}

// --- pages ---

void Launcher::page_play() {
    const Texts& t = T();
    const bool ready = game_ready_ && program_ready_;
    title(ready ? t.ready_title : t.setup_title);
    lead(ready ? t.ready_lead : t.setup_lead);
    std::string detail = "Uncharted: Golden Abyss \xC2\xB7 PCSA00029";
    if (!game_version_.empty()) detail += " \xC2\xB7 " + game_version_;
    if (tools_ready_ && !ready) {   // a release: the launcher makes the game itself
        if (game_ready_) status(true, t.game_ok, detail.c_str(), nullptr, "##game");
        setup_rows();
        return;
    }
    status(game_ready_, game_ready_ ? t.game_ok : t.game_missing, game_ready_ ? detail.c_str() : t.game_missing_detail, nullptr, "##game", !game_ready_);
    if (!program_ready_) status(false, t.program_missing, t.program_missing_detail, nullptr, "##program", true);
    status(pad_connected_, pad_connected_ ? t.pad_on : t.pad_off, pad_connected_ ? t.pad_on_detail : t.pad_off_detail, nullptr, "##pad");
    const int current = style();
    const std::string heading = std::string(t.style) + ": " + t.style_names[current];
    if (status(true, heading.c_str(), t.style_details[current], t.change, "##style")) {
        page_ = Page::kSettings;
        menu_cursor_ = 1;
        page_since_ = static_cast<float>(ImGui::GetTime());
    }
    cursor_ = std::max(cursor_, view_bottom_ - origin_.y - px(130));  // Play at the foot of the column
    if (ready && big_button(t.play, "##play", true)) play_ = true;
}

void Launcher::page_settings() {
    const Texts& t = T();
    title(t.settings);
    style_row();
    const char* tab_names[] = {t.tab_picture, t.tab_effects, t.tab_game, t.tab_audio};
    tabs(tab_names, 4, &settings_tab_);
    const char* off_on[] = {"0", "1"};
    const char* no_yes[] = {t.no, t.yes};
    if (settings_tab_ == 0) {
        const char* aspect_values[] = {"native", "21:9", "32:9", "window"};
        row(t.aspect, t.aspect_detail, 330);
        choice("aspect_ratio", "native", aspect_values, t.aspect_names, 4, nullptr, 330);
        row(t.window_mode, t.window_detail);
        choice("fullscreen", "0", off_on, t.window_names, 2);
        const char* scale_values[] = {"1", "2", "3", "4", "5", "6"};
        const char* scale_labels[] = {"1x", "2x", "3x", "4x", "5x", "6x"};
        const char* scale_details[] = {"544p", "1088p", "1632p", "2176p", "2720p", "3264p"};
        row(t.resolution, t.resolution_detail, 400);
        choice("render_scale", "1", scale_values, scale_labels, 6, scale_details, 400);
        const char* rate_values[] = {"30", "60"};
        row(t.frame_rate, t.frame_rate_detail);
        choice("frame_rate", "30", rate_values, rate_values, 2);
        const char* filter_values[] = {"1", "4", "8", "16"};
        const char* filter_labels[] = {"1x", "4x", "8x", "16x"};
        row(t.anisotropy, nullptr);
        choice("anisotropy", "16", filter_values, filter_labels, 4);
    } else if (settings_tab_ == 1) {
        row(t.fsr, t.fsr_detail);
        choice("fsr", "0", off_on, no_yes, 2);
        row(t.post_aa, t.post_aa_detail);
        choice("post_aa", "1", off_on, no_yes, 2);
        row(t.sharpen, nullptr);
        number("sharpen", 0.35f, 0, 1, "%.2f");
        row(t.ssao, t.ssao_detail);
        choice("ssao", "1", off_on, no_yes, 2);
        const char* shadow_values[] = {"0", "1", "2"};
        row(t.shadows, nullptr, 330);
        choice("shadow_filter", "1", shadow_values, t.shadow_names, 3, nullptr, 330);
    } else if (settings_tab_ == 2) {
        row(t.fov, t.fov_detail);
        number("fov", 1.0f, 0.8f, 1.5f, "%.2f");
        row(t.view_distance, t.view_distance_detail);
        number("view_distance", 1.0f, 1.0f, 3.0f, "%.2f");
        const char* language_values[] = {"auto", "it", "en", "pt", "es"};
        row(t.language, nullptr, 420);
        choice("launcher_language", "auto", language_values, t.language_names, 5, nullptr, 420);
    } else {
        row(t.volume, nullptr);
        number("volume", 1.0f, 0, 1, "%.2f");
    }
}

void Launcher::page_controls() {
    const Texts& t = T();
    title(t.controls);
    lead(t.controls_lead);
    const char* tab_names[] = {t.tab_keys, t.tab_mouse, t.tab_how};
    if (tabs(tab_names, 3, &controls_tab_)) rebinding_ = -1;
    if (controls_tab_ == 0) {
        // Two columns of actions: the console's button, what it does, the key (click, then press the new one).
        const float column = panel_width_ / 2, line = px(40);
        for (int i = 0; i < 16; ++i) {
            const int col = i % 2, r = i / 2;
            const ImVec2 at(origin_.x + column * static_cast<float>(col), origin_.y + cursor_ + line * static_cast<float>(r));
            const float icon = glyph(kBinds[i].glyph, ImVec2(at.x, at.y + px(4)), px(24));
            list_->PushClipRect(at, ImVec2(at.x + column - px(92), at.y + line), true);
            text(body_, px(17), ImVec2(at.x + std::max(icon, px(38)) + px(10), at.y + px(7)), kInk, t.actions[i]);
            list_->PopClipRect();
            const std::string name = setting(kBinds[i].key, kBinds[i].def);
            const bool waiting = rebinding_ == i;
            const ImVec2 box_a(at.x + column - px(88), at.y + px(2)), box_b(at.x + column - px(12), at.y + px(34));
            ImGui::SetCursorScreenPos(box_a);
            ImGui::PushID(i);
            claim_focus();
            if (ImGui::InvisibleButton("##key", ImVec2(box_b.x - box_a.x, box_b.y - box_a.y), kNavigable) && rebinding_ < 0) rebinding_ = i;
            const float hot = anim(ImGui::GetItemID(), ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride) || nav_selected());
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            focus_frame(ImVec2(at.x - px(6), at.y), ImVec2(at.x + column - px(6), at.y + line - px(4)));
            ImGui::PopID();
            list_->AddRectFilled(box_a, box_b, fade(waiting ? kGoldDark : mix(IM_COL32(255, 255, 255, 22), kGold, hot * 0.35f)));
            const char* shown = waiting ? "..." : name.empty() ? "-" : name.c_str();
            const ImVec2 size = measure(body_bold_, px(16), shown);
            text(body_bold_, px(16), ImVec2(box_b.x - size.x - px(8), box_a.y + px(7)), waiting ? IM_COL32_WHITE : kInk, shown);
        }
        cursor_ += line * 8 + px(10);
        if (rebinding_ >= 0) text(body_bold_, px(18), ImVec2(origin_.x + px(200), origin_.y + cursor_ + px(6)), kAmber, t.press_key);
        if (small_button(t.default_keys, ImVec2(origin_.x, origin_.y + cursor_), "##defaults")) {
            for (const Bind& bind : kBinds) settings_set_str(bind.key, bind.def);
            for (const MouseBind& bind : kMouseBinds) settings_set_str(bind.key, bind.def);
            settings_save();
            rebinding_ = -1;
        }
        cursor_ += px(44);
    } else if (controls_tab_ == 1) {
        const char* mode_values[] = {"auto", "pad", "kbm"};
        row(t.input_mode, t.input_mode_detail, 400);
        choice("input_mode", "auto", mode_values, t.input_names, 3, nullptr, 400);
        // Each mouse button on one action of the keyboard table, or on none.
        text(body_, px(17), ImVec2(origin_.x, origin_.y + cursor_), kDim, t.mouse_lead, panel_width_);
        cursor_ += measure(body_, px(17), t.mouse_lead, panel_width_).y + px(10);
        for (int m = 0; m < 5; ++m) {
            const std::string now = setting(kMouseBinds[m].key, kMouseBinds[m].def);
            int action = -1;
            for (int i = 0; i < 16; ++i) {
                if (now == kBinds[i].key + 5) action = i;  // ("bind_" + name)
            }
            const ImVec2 at(origin_.x, origin_.y + cursor_);
            text(body_bold_, px(18), ImVec2(at.x, at.y + px(7)), kInk, t.mouse_buttons[m]);
            const ImVec2 box_a(at.x + px(190), at.y + px(2)), box_b(at.x + panel_width_, at.y + px(34));
            ImGui::SetCursorScreenPos(box_a);
            ImGui::PushID(100 + m);
            claim_focus();
            const bool pressed = ImGui::InvisibleButton("##mouse", ImVec2(box_b.x - box_a.x, box_b.y - box_a.y), kNavigable);
            const bool back = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
            const float hot = anim(ImGui::GetItemID(), ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride) || nav_selected());
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            const int step = pressed ? 1 : back ? -1 : value_step();
            focus_frame(ImVec2(at.x - px(24), at.y), ImVec2(box_b.x + px(14), at.y + px(36)));
            ImGui::PopID();
            if (step) {  // none, then the sixteen actions, around
                action = (action + 1 + step + 17) % 17 - 1;
                settings_set_str(kMouseBinds[m].key, action < 0 ? "" : kBinds[action].key + 5);
                settings_save();
                value_moved_frame_ = ImGui::GetFrameCount();
            }
            list_->AddRectFilled(box_a, box_b, fade(mix(IM_COL32(255, 255, 255, 22), kGold, hot * 0.35f)));
            float x = box_a.x + px(10);
            if (action >= 0) x += glyph(kBinds[action].glyph, ImVec2(x, box_a.y + px(5)), px(22)) + px(10);
            text(body_, px(17), ImVec2(x, box_a.y + px(6)), action < 0 ? kDim : kInk, action < 0 ? t.none : t.actions[action]);
            cursor_ += px(40);
        }
        cursor_ += px(8);
        row(t.mouse_sens, nullptr);
        number("mouse_sens", 1.0f, 0.1f, 5.0f, "%.2f");
        const char* off_on[] = {"0", "1"};
        const char* no_yes[] = {t.no, t.yes};
        row(t.mouse_invert, nullptr);
        choice("mouse_invert", "0", off_on, no_yes, 2);
        const char* shoulder_values[] = {"triggers", "bumpers"};
        row(t.shoulders, t.shoulders_detail);
        choice("shoulder_buttons", "triggers", shoulder_values, t.shoulder_names, 2);
        row(t.gyro, t.gyro_detail);
        choice("gyro", "1", off_on, no_yes, 2);
    } else {
        how_page(static_cast<float>(ImGui::GetTime()));
    }
}

// "How to play", a tutorial for the device chosen at its top (keyboard and mouse, a DualSense, an Xbox controller):
// each of the console's inputs as a small moving scene -- what the player does with that device on the left, what
// the console sees on the right -- with its name and what to do under it. The scenes follow one another by
// themselves; pointing at a name, or choosing it, shows that one.
void Launcher::how_page(float now) {
    const Texts& t = T();
    constexpr int kCount = 9;
    constexpr float kLength = 3.6f;  // seconds a scene's loop lasts
    text(body_, px(17), ImVec2(origin_.x, origin_.y + cursor_), kDim, t.how_lead, panel_width_);
    cursor_ += measure(body_, px(17), t.how_lead, panel_width_).y + px(4);

    // What the tutorial is for: chosen here, first guessed from the controller connected (a PlayStation one: the
    // DualSense's; any other: the Xbox's; none: RecompLauncher's {pad} if it gave one, else keyboard and mouse).
    if (how_device_ < 0) {
        const char* launcher_pad = std::getenv("RECOMPLAUNCHER_PAD");
        how_device_ = !launcher_pad ? 0 : !std::strcmp(launcher_pad, "playstation") ? 1 : !std::strcmp(launcher_pad, "xbox") ? 2 : 0;
        int count = 0;
        if (SDL_JoystickID* pads = SDL_GetGamepads(&count)) {
            if (count > 0) {
                const SDL_GamepadType type = SDL_GetGamepadTypeForID(pads[0]);
                how_device_ = type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 || type == SDL_GAMEPAD_TYPE_PS5 ? 1 : 2;
            }
            SDL_free(pads);
        }
    }
    const int device = how_device_;
    if (segmented("how_device", t.how_devices, 3, &how_device_, nullptr, panel_width_ / px(1))) how_since_ = now;
    const bool sony = device == 1;
    const bool bumpers = setting("shoulder_buttons", "triggers") == "bumpers";   // L and R on the bumpers: the triggers hold the touch
    // Keyboard and mouse as this player set them on the other two tabs: the keys that move left and right (they tilt),
    // and what is L (aim) and R (fire): a mouse button given that action, else its key. Alt, F1 and F7 are fixed.
    const std::string move_left = setting("bind_move_left", "A"), move_right = setting("bind_move_right", "D");
    int aim_button = -1, fire_button = -1;
    for (int m = 4; m >= 0; --m) {   // (the first button with the action wins, as in the runtime)
        const std::string action = setting(kMouseBinds[m].key, kMouseBinds[m].def);
        if (action == "l") aim_button = m;
        if (action == "r") fire_button = m;
    }
    const std::string aim_key = setting("bind_l", "Q"), fire_key = setting("bind_r", "R");

    if (now >= how_hold_until_ && now - how_since_ >= kLength * 2) {  // two loops each, then the next
        how_item_ = (how_item_ + 1) % kCount;
        how_since_ = now;
    }
    const float p = std::fmod(std::max(0.0f, now - how_since_), kLength) / kLength;  // 0..1 through the loop
    const float turn = p * 6.2831853f;

    // The stage.
    const ImVec2 a(origin_.x, origin_.y + cursor_), b(origin_.x + panel_width_, a.y + px(200));
    list_->AddRectFilled(a, b, fade(IM_COL32(255, 255, 255, 10)));
    list_->AddRect(a, b, fade(IM_COL32(255, 255, 255, 26)), 0, 0, px(1));
    const ImVec2 left(a.x + px(130), (a.y + b.y) / 2), right(b.x - px(180), (a.y + b.y) / 2);
    const ImU32 dark = IM_COL32(34, 32, 30, 255), edge = IM_COL32(255, 255, 255, 80), screen_colour = IM_COL32(12, 22, 30, 255);

    // What the scenes are drawn with.
    const auto key = [&](ImVec2 c, const char* label, bool down) {
        const ImVec2 size = measure(body_bold_, px(18), label);
        const float w = std::max(px(46), size.x + px(22)), h = px(42), sink = down ? px(3) : 0;
        const ImVec2 ka(c.x - w / 2, c.y - h / 2 + sink), kb(c.x + w / 2, c.y + h / 2 + sink);
        list_->AddRectFilled(ImVec2(ka.x, ka.y + px(4) - sink), ImVec2(kb.x, kb.y + px(4) - sink), fade(IM_COL32(0, 0, 0, 120)), px(7));
        list_->AddRectFilled(ka, kb, fade(down ? kGold : dark), px(7));
        list_->AddRect(ka, kb, fade(down ? IM_COL32(255, 232, 170, 255) : edge), px(7), 0, px(1.5f));
        text(body_bold_, px(18), ImVec2(c.x - size.x / 2, c.y - size.y / 2 + sink), down ? IM_COL32(18, 14, 6, 255) : kInk, label);
    };
    const auto mouse = [&](ImVec2 c, bool left_down, bool right_down) {
        const float w = px(54), h = px(84);
        const ImVec2 ma(c.x - w / 2, c.y - h / 2), mb(c.x + w / 2, c.y + h / 2);
        list_->AddRectFilled(ma, mb, fade(dark), px(24));
        if (left_down) list_->AddRectFilled(ma, ImVec2(c.x, ma.y + h * 0.42f), fade(kGold), px(24), ImDrawFlags_RoundCornersTopLeft);
        if (right_down) list_->AddRectFilled(ImVec2(c.x, ma.y), ImVec2(mb.x, ma.y + h * 0.42f), fade(kAmber), px(24), ImDrawFlags_RoundCornersTopRight);
        list_->AddRect(ma, mb, fade(edge), px(24), 0, px(1.5f));
        list_->AddLine(ImVec2(c.x, ma.y), ImVec2(c.x, ma.y + h * 0.42f), fade(edge), px(1.5f));
        list_->AddLine(ImVec2(ma.x, ma.y + h * 0.42f), ImVec2(mb.x, ma.y + h * 0.42f), fade(edge), px(1.5f));
    };
    const float half = px(84), half_h = px(47);
    const auto rotated = [&](ImVec2 c, ImVec2 offset, float angle) {
        const float cs = std::cos(angle), sn = std::sin(angle);
        return ImVec2(c.x + offset.x * cs - offset.y * sn, c.y + offset.x * sn + offset.y * cs);
    };
    // The console, level or tilted by `angle`; `rear`: seen from the back, its touch pad where the screen would be.
    const auto console = [&](ImVec2 c, float angle, bool rear) {
        const float bw = px(150), bh = px(62);
        const auto box = [&](float x0, float y0, float x1, float y1, ImU32 colour) {
            list_->AddQuadFilled(rotated(c, ImVec2(x0, y0), angle), rotated(c, ImVec2(x1, y0), angle), rotated(c, ImVec2(x1, y1), angle),
                                 rotated(c, ImVec2(x0, y1), angle), fade(colour));
        };
        box(-bw + bh, -bh, bw - bh, bh, IM_COL32(26, 26, 30, 255));
        list_->AddCircleFilled(rotated(c, ImVec2(-bw + bh, 0), angle), bh, fade(IM_COL32(26, 26, 30, 255)), 32);
        list_->AddCircleFilled(rotated(c, ImVec2(bw - bh, 0), angle), bh, fade(IM_COL32(26, 26, 30, 255)), 32);
        if (rear) {
            box(-half, -half_h, half, half_h, IM_COL32(40, 40, 46, 255));
            for (int i = 0; i < 9; ++i) {  // the rear pad's pattern
                for (int j = 0; j < 5; ++j) {
                    list_->AddCircleFilled(rotated(c, ImVec2(-half + px(18) * static_cast<float>(i) + px(12), -half_h + px(18) * static_cast<float>(j) + px(11)), angle),
                                           px(1.6f), fade(IM_COL32(255, 255, 255, 50)));
                }
            }
        } else {
            box(-half, -half_h, half, half_h, screen_colour);
            list_->AddCircle(rotated(c, ImVec2(-bw + px(26), px(22)), angle), px(11), fade(edge), 0, px(1.5f));  // sticks
            list_->AddCircle(rotated(c, ImVec2(bw - px(26), px(22)), angle), px(11), fade(edge), 0, px(1.5f));
            list_->AddCircleFilled(rotated(c, ImVec2(-bw + px(30), -px(16)), angle), px(9), fade(IM_COL32(255, 255, 255, 40)));  // d-pad
            for (int i = 0; i < 4; ++i) {  // the four buttons
                const ImVec2 at(bw - px(30) + px(9) * (i == 1 ? 1.0f : i == 3 ? -1.0f : 0.0f), -px(16) + px(9) * (i == 0 ? -1.0f : i == 2 ? 1.0f : 0.0f));
                list_->AddCircleFilled(rotated(c, at, angle), px(3.2f), fade(IM_COL32(255, 255, 255, 110)));
            }
        }
        return c;
    };
    const auto finger = [&](ImVec2 at, bool down, ImU32 colour) {
        list_->AddCircleFilled(at, px(down ? 7 : 9), fade(colour, down ? 1.0f : 0.55f));
        list_->AddCircle(at, px(down ? 7 : 9), fade(IM_COL32_WHITE, 0.8f), 0, px(1.5f));
    };
    const auto ripple = [&](ImVec2 at, float q, ImU32 colour) {
        if (q > 0 && q < 1) list_->AddCircle(at, px(8) + px(26) * q, fade(colour, 1 - q), 0, px(2.5f));
    };
    const auto arrow = [&](ImVec2 from, ImVec2 to, ImU32 colour) {
        list_->AddLine(from, to, fade(colour), px(2.5f));
        const float length = std::sqrt((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y));
        if (length < 1) return;
        const ImVec2 d((to.x - from.x) / length, (to.y - from.y) / length);
        list_->AddTriangleFilled(to, ImVec2(to.x - d.x * px(10) - d.y * px(6), to.y - d.y * px(10) + d.x * px(6)),
                                 ImVec2(to.x - d.x * px(10) + d.y * px(6), to.y - d.y * px(10) - d.x * px(6)), fade(colour));
    };

    // A controller: one standard drawing for the DualSense and the Xbox alike (art/pad.png, 512 x 417 pixels; the places
    // below are its pixels), with the face buttons' symbols drawn on it: the PlayStation shapes or A B X Y. What the
    // scene presses lights up in gold.
    struct Pose {
        ImVec2 ls{}, rs{};
        bool l1 = false, r1 = false, l2 = false, r2 = false, south = false, back = false, start = false;
        float tilt = 0;
    };
    const auto pad = [&](ImVec2 c, const Pose& s) {
        const float width = px(214), k = width / 512;   // on screen
        const auto at = [&](float u, float v) { return rotated(c, ImVec2((u - 256) * k, (v - 208.5f) * k), s.tilt); };
        const ImU32 ink = IM_COL32(26, 23, 20, 255);

        // the shoulders: a small rounded trigger above, named on it; the bumper on the drawing's own, named beside it
        const auto shoulder = [&](float u0, float v0, float u1, float v1, float round, bool on, const char* name, int side, ImU32 off) {
            const float sink = on ? 3.0f : 0.0f;
            ImVec2 points[24];
            int count = 0;
            for (int corner = 0; corner < 4; ++corner) {   // a rounded rectangle, its corners as quarter circles
                const float cu = corner == 0 || corner == 3 ? u0 + round : u1 - round, cv = corner < 2 ? v0 + round : v1 - round;
                for (int j = 0; j < 6; ++j) {
                    const float angle = 3.14159265f * (1.0f + 0.5f * static_cast<float>(corner) + 0.1f * static_cast<float>(j));
                    points[count++] = at(cu + std::cos(angle) * round, cv + std::sin(angle) * round + sink);
                }
            }
            list_->AddConvexPolyFilled(points, count, fade(on ? kGold : off));
            list_->AddPolyline(points, count, fade(on ? IM_COL32(255, 228, 160, 255) : ink), ImDrawFlags_Closed, px(1.5f));
            const ImVec2 label = measure(body_bold_, px(11), name);
            if (side == 0) {
                const ImVec2 middle = at((u0 + u1) / 2, (v0 + v1) / 2 + sink);
                text(body_bold_, px(11), ImVec2(middle.x - label.x / 2, middle.y - label.y / 2), on ? IM_COL32(26, 18, 4, 255) : IM_COL32(210, 210, 218, 255), name);
            } else {
                const ImVec2 end = at(side < 0 ? u0 - 10 : u1 + 10, (v0 + v1) / 2);
                text(body_bold_, px(11), ImVec2(side < 0 ? end.x - label.x : end.x, end.y - label.y / 2), on ? kGold : IM_COL32(190, 190, 198, 255), name);
            }
        };
        static const char* const kShoulderNames[2][4] = {{"L1", "R1", "L2", "R2"}, {"LB", "RB", "LT", "RT"}};
        const char* const* names = kShoulderNames[sony ? 0 : 1];
        shoulder(110, 34, 166, 72, 10, s.l2, names[2], 0, IM_COL32(74, 74, 84, 255));   // the triggers
        shoulder(346, 34, 402, 72, 10, s.r2, names[3], 0, IM_COL32(74, 74, 84, 255));
        if (pad_art_) {
            list_->AddImageQuad(ImTextureRef(reinterpret_cast<ImTextureID>(pad_art_)), at(0, 0), at(512, 0), at(512, 417), at(0, 417),
                                ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1), fade(IM_COL32_WHITE));
        }
        // the bumpers are the drawing's own: a pressed one lights up through art/pad_bumpers.png (its left half L1/LB,
        // its right half R1/RB); their names beside them
        for (int side = 0; side < 2; ++side) {
            const bool on = side ? s.r1 : s.l1;
            if (on && pad_bumpers_) {
                const float u0 = side ? 256.0f : 0.0f, u1 = side ? 512.0f : 256.0f;
                list_->AddImageQuad(ImTextureRef(reinterpret_cast<ImTextureID>(pad_bumpers_)), at(u0, 0), at(u1, 0), at(u1, 417), at(u0, 417),
                                    ImVec2(u0 / 512, 0), ImVec2(u1 / 512, 0), ImVec2(u1 / 512, 1), ImVec2(u0 / 512, 1), fade(kGold));
            }
            const ImVec2 size = measure(body_bold_, px(11), names[side]);
            const ImVec2 end = at(side ? 434.0f : 78.0f, 100.0f);
            text(body_bold_, px(11), ImVec2(side ? end.x : end.x - size.x, end.y - size.y / 2), on ? kGold : IM_COL32(190, 190, 198, 255), names[side]);
        }

        const auto stick = [&](ImVec2 hole, ImVec2 move) {   // a stick pushed: its cap, in gold, off the centre
            if (move.x * move.x + move.y * move.y < 0.04f) return;
            const ImVec2 cap = at(hole.x + move.x * 9, hole.y + move.y * 9);
            list_->AddCircleFilled(cap, k * 26, fade(kGold), 28);
            list_->AddCircle(cap, k * 26, fade(ink), 28, px(1.5f));
            list_->AddCircleFilled(cap, k * 10, fade(ink), 16);
        };
        stick(ImVec2(129.4f, 174.5f), s.ls);
        stick(ImVec2(323.1f, 253.7f), s.rs);

        // the face buttons: Y/triangle top, B/circle right, A/cross bottom, X/square left
        static const ImVec2 kFace[4] = {{384.2f, 147.0f}, {416.2f, 179.0f}, {384.2f, 211.0f}, {352.2f, 179.0f}};
        static const char* const kLetter[4] = {"Y", "B", "A", "X"};
        for (int i = 0; i < 4; ++i) {
            const ImVec2 q = at(kFace[i].x, kFace[i].y);
            const bool lit = i == 2 && s.south;
            if (lit) list_->AddCircleFilled(q, k * 15, fade(kGold), 20);
            const ImU32 colour = fade(lit ? IM_COL32(26, 18, 4, 255) : ink);
            const float r = k * 7.5f, line = px(1.6f);
            if (!sony) {
                const ImVec2 size = measure(body_bold_, px(12), kLetter[i]);
                text(body_bold_, px(12), ImVec2(q.x - size.x / 2, q.y - size.y / 2), lit ? IM_COL32(26, 18, 4, 255) : ink, kLetter[i]);
            } else if (i == 0) {
                list_->AddTriangle(ImVec2(q.x, q.y - r), ImVec2(q.x + r, q.y + r * 0.75f), ImVec2(q.x - r, q.y + r * 0.75f), colour, line);
            } else if (i == 1) {
                list_->AddCircle(q, r, colour, 16, line);
            } else if (i == 2) {
                list_->AddLine(ImVec2(q.x - r, q.y - r), ImVec2(q.x + r, q.y + r), colour, line);
                list_->AddLine(ImVec2(q.x + r, q.y - r), ImVec2(q.x - r, q.y + r), colour, line);
            } else {
                list_->AddRect(ImVec2(q.x - r * 0.85f, q.y - r * 0.85f), ImVec2(q.x + r * 0.85f, q.y + r * 0.85f), colour, 0, 0, line);
            }
        }
        // the two small buttons in the middle: Create and Options, or View and Menu
        if (s.back) list_->AddCircleFilled(at(231.9f, 189.1f), k * 10, fade(kGold), 14);
        if (s.start) list_->AddCircleFilled(at(280.1f, 189.1f), k * 10, fade(kGold), 14);
    };

    const float span = std::clamp((p - 0.15f) / 0.6f, 0.0f, 1.0f);  // a movement in the loop's middle
    const float eased = ease_out(span);
    const bool kbm = device == 0;
    Pose pose;
    bool show_pad = !kbm;

    list_->PushClipRect(a, b, true);
    switch (how_item_) {
        case 0: {  // looking around: the mouse or the right stick moves, the view turns
            const float sway = std::sin(turn);
            if (kbm) {
                mouse(ImVec2(left.x + px(34) * sway, left.y), false, false);
                arrow(ImVec2(left.x - px(64), left.y + px(62)), ImVec2(left.x - px(92), left.y + px(62)), kDim);
                arrow(ImVec2(left.x + px(64), left.y + px(62)), ImVec2(left.x + px(92), left.y + px(62)), kDim);
            }
            pose.rs = ImVec2(sway, 0);
            const ImVec2 c = console(right, 0, false);
            list_->PushClipRect(ImVec2(c.x - half, c.y - half_h), ImVec2(c.x + half, c.y + half_h), true);
            const float shift = -sway * px(46);
            for (int i = -2; i <= 3; ++i) {  // a skyline that slides the other way
                const float x = c.x + shift + px(62) * static_cast<float>(i);
                const float top = c.y - px(i % 2 ? 26 : 8);
                list_->AddTriangleFilled(ImVec2(x - px(44), c.y + half_h), ImVec2(x, top), ImVec2(x + px(44), c.y + half_h), fade(i % 2 ? IM_COL32(40, 78, 62, 255) : IM_COL32(58, 104, 76, 255)));
            }
            list_->AddCircleFilled(ImVec2(c.x + shift * 0.4f + px(36), c.y - px(26)), px(9), fade(IM_COL32(255, 214, 140, 255)));
            list_->PopClipRect();
            break;
        }
        case 1:    // touching the screen
        case 2: {  // the rear pad
            const bool rear = how_item_ == 2;
            const bool pressed = p > 0.55f && p < 0.9f;
            const float q = ease_out(std::clamp(p / 0.5f, 0.0f, 1.0f));
            if (kbm) {
                key(ImVec2(left.x - px(58), left.y), "Alt", true);
                mouse(ImVec2(left.x + px(40), left.y), pressed && !rear, pressed && rear);
            } else {  // the cursor: a shoulder held, the stick moves it, Cross / A touches (a DualSense's touchpad: the text)
                (rear ? (bumpers ? pose.r2 : pose.r1) : (bumpers ? pose.l2 : pose.l1)) = true;
                pose.ls = p < 0.5f ? ImVec2(0.7f, -0.7f) : ImVec2();
                pose.south = pressed;
            }
            const ImVec2 c = console(right, 0, rear);
            const ImVec2 from(c.x - half + px(14), c.y + half_h - px(12)), to(c.x + px(22), c.y - px(8));
            const ImVec2 at(from.x + (to.x - from.x) * q, from.y + (to.y - from.y) * q);
            ripple(at, (p - 0.55f) / 0.35f, rear ? kAmber : kGold);
            finger(at, pressed, rear ? kAmber : kGold);
            break;
        }
        case 3: {  // rubbing, drawing: a touch dragged
            const bool pressed = p > 0.12f && p < 0.8f;
            const float wave = std::cos(span * 18.85f);
            if (kbm) {
                key(ImVec2(left.x - px(58), left.y), "Alt", true);
                mouse(ImVec2(left.x + px(40) + px(14) * std::sin(turn * 3), left.y), pressed, false);
            } else {
                (bumpers ? pose.l2 : pose.l1) = true;
                pose.south = pressed;
                pose.ls = pressed ? ImVec2(0.6f, 0.9f * wave) : ImVec2();
            }
            const ImVec2 c = console(right, 0, false);
            ImVec2 last(c.x - half + px(16), c.y);
            const int steps = static_cast<int>(48 * span);
            for (int i = 1; i <= steps; ++i) {
                const float v = static_cast<float>(i) / 48;
                const ImVec2 point(c.x - half + px(16) + (2 * half - px(32)) * v, c.y + px(26) * std::sin(v * 18.85f));
                list_->AddLine(last, point, fade(kGold), px(3));
                last = point;
            }
            finger(last, pressed, kGold);
            break;
        }
        case 4: {  // swiping: a drag with the left button, or a flick of the right stick
            const bool pressed = p > 0.15f && p < 0.75f;
            if (kbm) mouse(ImVec2(left.x, left.y + px(26) - px(52) * eased), pressed, false);
            else pose.rs = ImVec2(0, p > 0.15f && p < 0.35f ? -1.0f : 0.0f);
            const ImVec2 c = console(right, 0, false);
            const ImVec2 from(c.x, c.y + half_h - px(12)), to(c.x, c.y + half_h - px(12) - (2 * half_h - px(24)) * eased);
            if (pressed) arrow(from, to, kGold);
            finger(to, pressed, kGold);
            break;
        }
        case 5: {  // tilting: A and D, the DualSense itself, or the left stick
            const float lean = std::sin(turn);
            if (kbm) {
                key(ImVec2(left.x - px(40), left.y), move_left.c_str(), lean < -0.2f);
                key(ImVec2(left.x + px(40), left.y), move_right.c_str(), lean > 0.2f);
            } else if (sony) {
                pose.tilt = 0.38f * lean;
            } else {
                pose.ls = ImVec2(lean, 0);
            }
            console(right, 0.38f * lean, false);
            break;
        }
        case 6: {  // the camera's lamp: F7, on any device
            const bool on = p > 0.3f && p < 0.8f;
            key(left, "F7", (p > 0.3f && p < 0.4f) || (p > 0.8f && p < 0.9f));
            show_pad = false;
            const ImVec2 c = console(right, 0, false);
            const float glow = anim(ImGui::GetID("##lamp"), on, 6);
            list_->AddCircleFilled(c, px(18) + px(26) * glow, fade(IM_COL32(255, 226, 150, 255), 0.25f * glow), 32);
            list_->AddCircleFilled(c, px(12) + px(8) * glow, fade(IM_COL32(255, 240, 200, 255), 0.25f + 0.75f * glow), 32);
            break;
        }
        case 7: {  // aiming and firing: the sight closes in while aiming, a flash on each shot
            const bool aiming = p > 0.15f && p < 0.9f;
            const bool firing = (p > 0.45f && p < 0.52f) || (p > 0.65f && p < 0.72f);
            if (kbm) {   // the mouse's left and right buttons where they hold L or R; a key cap for the rest
                static const char* const kExtra[5] = {"", "", "M3", "M4", "M5"};   // (middle and side buttons)
                const auto cap = [&](int which) {   // 0 aim, 1 fire: its key's or button's name
                    const int button = which ? fire_button : aim_button;
                    return button >= 2 ? std::string(kExtra[button]) : which ? fire_key : aim_key;
                };
                const bool aim_on_mouse = aim_button == 0 || aim_button == 1, fire_on_mouse = fire_button == 0 || fire_button == 1;
                std::vector<int> caps;
                if (!aim_on_mouse) caps.push_back(0);
                if (!fire_on_mouse) caps.push_back(1);
                const bool any_mouse = aim_on_mouse || fire_on_mouse;
                float x = left.x - (any_mouse ? px(40) : px(40) * static_cast<float>(caps.size() - 1));
                for (const int which : caps) {
                    key(ImVec2(x, left.y), cap(which).c_str(), which ? firing : aiming);
                    x += px(80);
                }
                if (any_mouse) {
                    mouse(ImVec2(caps.empty() ? left.x : left.x + px(46), left.y), (aim_button == 0 && aiming) || (fire_button == 0 && firing),
                          (aim_button == 1 && aiming) || (fire_button == 1 && firing));
                }
            } else {
                (bumpers ? pose.l1 : pose.l2) = aiming;
                (bumpers ? pose.r1 : pose.r2) = firing;
            }
            const ImVec2 c = console(right, 0, false);
            const float gap = px(aiming ? 8 : 18) + (aiming ? 0 : px(4) * std::sin(turn * 2));
            const ImU32 sight = fade(aiming ? kGold : IM_COL32(255, 255, 255, 160));
            for (int i = 0; i < 4; ++i) {
                const ImVec2 d(i == 0 ? 1.0f : i == 1 ? -1.0f : 0.0f, i == 2 ? 1.0f : i == 3 ? -1.0f : 0.0f);
                list_->AddLine(ImVec2(c.x + d.x * gap, c.y + d.y * gap), ImVec2(c.x + d.x * (gap + px(10)), c.y + d.y * (gap + px(10))), sight, px(2));
            }
            if (firing) list_->AddCircleFilled(c, px(14), fade(IM_COL32(255, 230, 160, 255), 0.6f), 20);
            break;
        }
        default: {  // the port's menu
            const bool press = p > 0.12f && p < 0.24f;
            if (kbm) key(left, "F1", press);
            else pose.back = pose.start = press;   // the two middle buttons together (or the PS / Xbox button: the text)
            const ImVec2 c = console(right, 0, false);
            const float open = ease_out(std::clamp((p - 0.2f) / 0.25f, 0.0f, 1.0f)) * (p < 0.9f ? 1.0f : 0.0f);
            if (open > 0.01f) {
                const ImVec2 ma(c.x - half + px(10), c.y - half_h + px(8)), mb(ma.x + (2 * half - px(20)) * open, c.y + half_h - px(8));
                list_->AddRectFilled(ma, mb, fade(IM_COL32(14, 12, 10, 235)));
                list_->AddRectFilled(ma, ImVec2(mb.x, ma.y + px(3)), fade(kGold));
                for (int i = 0; i < 4 && open > 0.8f; ++i) {
                    list_->AddRectFilled(ImVec2(ma.x + px(10), ma.y + px(14) + px(15) * static_cast<float>(i)),
                                         ImVec2(ma.x + px(i == 1 ? 110 : 80), ma.y + px(21) + px(15) * static_cast<float>(i)), fade(i == 1 ? kGold : IM_COL32(255, 255, 255, 70)));
                }
            }
            break;
        }
    }
    if (show_pad) pad(ImVec2(left.x, left.y + px(12)), pose);   // (its shoulders rise above the body: lower, centred)
    list_->PopClipRect();
    arrow(ImVec2(a.x + px(250), (a.y + b.y) / 2), ImVec2(a.x + px(282), (a.y + b.y) / 2), IM_COL32(255, 255, 255, 60));
    cursor_ += px(200) + px(12);

    // The scene's name and its explanation, with the shoulder buttons this player's settings give.
    std::string explanation = t.how_text[device][how_item_];
    const char* const names[2][4] = {{"L1", "R1", "L2", "R2"}, {"LB", "RB", "LT", "RT"}};
    const char* const* n = names[sony ? 0 : 1];
    const std::pair<const char*, const char*> marks[] = {{"{TR}", bumpers ? n[3] : n[1]}, {"{T}", bumpers ? n[2] : n[0]},
                                                         {"{A}", bumpers ? n[0] : n[2]}, {"{F}", bumpers ? n[1] : n[3]}};
    // keyboard and mouse: the keys and buttons set on the other tabs
    const std::string aim_name = aim_button >= 0 ? t.mouse_buttons[aim_button] : aim_key.empty() ? t.none : aim_key;
    const std::string fire_name = fire_button >= 0 ? t.mouse_buttons[fire_button] : fire_key.empty() ? t.none : fire_key;
    const std::pair<const char*, std::string> kbm_marks[] = {{"{ML}", move_left}, {"{MR}", move_right}, {"{AIM}", aim_name}, {"{FIRE}", fire_name}};
    for (const auto& [mark, name] : marks) {
        for (size_t at; (at = explanation.find(mark)) != std::string::npos;) explanation.replace(at, std::strlen(mark), name);
    }
    for (const auto& [mark, name] : kbm_marks) {
        for (size_t at; (at = explanation.find(mark)) != std::string::npos;) explanation.replace(at, std::strlen(mark), name);
    }
    text(body_bold_, px(21), ImVec2(origin_.x, origin_.y + cursor_), kInk, t.how_titles[how_item_]);
    text(body_, px(17), ImVec2(origin_.x, origin_.y + cursor_ + px(28)), kDim, explanation.c_str(), panel_width_);
    cursor_ += px(28) + std::max(px(48), measure(body_, px(17), explanation.c_str(), panel_width_).y + px(14));

    // Every scene by name, three to a line: pointing at one shows it and holds it a while.
    const float cell = panel_width_ / 3, line = px(38);
    for (int i = 0; i < kCount; ++i) {
        const ImVec2 at(origin_.x + cell * static_cast<float>(i % 3), origin_.y + cursor_ + line * static_cast<float>(i / 3));
        ImGui::SetCursorScreenPos(at);
        ImGui::PushID(200 + i);
        const bool pressed = ImGui::InvisibleButton("##how", ImVec2(cell - px(8), line - px(6)), kNavigable);
        const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride), selected = nav_selected();
        const float hot = anim(ImGui::GetItemID(), hovered || selected);
        const float on = anim(ImGui::GetItemID() + 1, i == how_item_);
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::PopID();
        if ((pressed || hovered || selected) && i != how_item_) {
            how_item_ = i;
            how_since_ = now;
        }
        if (pressed || hovered || selected) how_hold_until_ = now + 12;
        const ImVec2 box_b(at.x + cell - px(8), at.y + line - px(6));
        list_->AddRectFilled(at, box_b, fade(mix(IM_COL32(255, 255, 255, 14), kGold, on * 0.9f + hot * 0.1f)));
        if (on > 0.5f) list_->AddRectFilled(at, ImVec2(at.x + (box_b.x - at.x) * std::min(1.0f, (now - how_since_) / (kLength * 2)), at.y + px(2)), fade(IM_COL32_WHITE, 0.7f));
        list_->PushClipRect(at, box_b, true);
        text(body_bold_, px(16), ImVec2(at.x + px(10), at.y + px(6)), mix(kInk, IM_COL32(18, 14, 6, 255), on), t.how_titles[i]);
        list_->PopClipRect();
    }
    cursor_ += line * 3 + px(4);
}

void Launcher::page_about() {
    const Texts& t = T();
    title(t.about);
    lead(t.about_lead);
    const auto fact = [&](const char* label, const char* value) {
        text(body_, px(17), ImVec2(origin_.x, origin_.y + cursor_), kDim, label);
        const float wrap = panel_width_ - px(150);
        text(body_bold_, px(18), ImVec2(origin_.x + px(150), origin_.y + cursor_ - px(1)), kInk, value, wrap);
        cursor_ += std::max(px(30), measure(body_bold_, px(18), value, wrap).y + px(12));
    };
    fact(t.version, ABYSS_VERSION);
    fact(t.licence, t.licence_text);   // (the credits are on GitHub: README.md, THIRD_PARTY.md)
    cursor_ += px(6);
    if (game_ready_) {
        text(body_, px(17), ImVec2(origin_.x, origin_.y + cursor_ + px(7)), kDim, t.folder);
        if (small_button(t.open_saves, ImVec2(origin_.x + px(150), origin_.y + cursor_), "##saves")) {
            std::error_code error;
            const fs::path saves = game_directory_ / "fs" / "savedata0";
            fs::create_directories(saves, error);
            ShellExecuteA(nullptr, "open", saves.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        cursor_ += px(48);
    }
}

// --- the screen ---

void Launcher::frame(float now, float dt) {
    const Texts& t = T();
    const ImVec2 screen(static_cast<float>(width_), static_cast<float>(height_));
    ImDrawList* back = ImGui::GetBackgroundDrawList();

    // The game's picture, blurred, filling the screen and drifting slowly; darkened so the texts read.
    back->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(10, 8, 6, 255));
    if (background_) {
        const float picture = static_cast<float>(background_width_) / static_cast<float>(std::max(1, background_height_));
        const float zoom = 1.08f + 0.03f * std::sin(now * 0.07f);
        float w = screen.x * zoom, h = w / picture;
        if (h < screen.y * zoom) {
            h = screen.y * zoom;
            w = h * picture;
        }
        const ImVec2 a((screen.x - w) / 2 + px(14) * std::sin(now * 0.05f), (screen.y - h) / 2);
        back->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(background_)), a, ImVec2(a.x + w, a.y + h));
    }
    back->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(8, 6, 4, 88));
    back->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(screen.x * 0.55f, screen.y), IM_COL32(0, 0, 0, 150), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 150));

    // Light trails and dust in the air.
    if (random01() < dt * 3) {
        const float pick = random01();
        const ImU32 colour = pick < 0.6f ? IM_COL32(226, 168, 44, 255) : pick < 0.85f ? IM_COL32(255, 226, 160, 255) : IM_COL32(255, 255, 255, 255);
        trails_.push_back({-px(300), screen.y * (0.55f + 0.37f * random01()), px(900 + 900 * random01()), px(200 + 320 * random01()), colour});
    }
    for (Trail& trail : trails_) {
        trail.x += trail.speed * dt;
        back->AddRectFilledMultiColor(ImVec2(trail.x - trail.length, trail.y), ImVec2(trail.x, trail.y + px(2.5f)),
                                      trail.colour & ~IM_COL32_A_MASK, trail.colour, trail.colour, trail.colour & ~IM_COL32_A_MASK);
    }
    trails_.erase(std::remove_if(trails_.begin(), trails_.end(), [&](const Trail& trail) { return trail.x - trail.length > screen.x; }), trails_.end());
    if (motes_.size() != 90) {
        motes_.clear();
        for (int i = 0; i < 90; ++i) motes_.push_back(ImVec4(screen.x * random01(), screen.y * random01(), 0.6f + 1.8f * random01(), 6.28f * random01()));
    }
    for (ImVec4& mote : motes_) {  // rising slowly, swaying
        mote.y -= px(8 + 10 * mote.z) * dt;
        mote.x += px(10) * std::sin(now * 0.6f + mote.w) * dt;
        if (mote.y < -px(10)) {
            mote.y = screen.y + px(10);
            mote.x = screen.x * random01();
        }
        back->AddCircleFilled(ImVec2(mote.x, mote.y), px(mote.z), IM_COL32(255, 214, 140, static_cast<int>(26 + 22 * mote.z)));
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(screen);
    ImGui::Begin("##launcher", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    list_ = ImGui::GetWindowDrawList();
    const float since = now - start_time_;
    header(since);
    const char* labels[] = {t.play, t.settings, t.controls, t.about, t.quit};
    const Page targets[] = {Page::kPlay, Page::kSettings, Page::kControls, Page::kAbout, Page::kQuit};
    side_menu(now, since, labels, targets, 5);
    panel(now);
    bottom_bar(since);
    g_alpha = 1;
    ImGui::End();
    after_frame(now);
}

// The game's logo (from the player's copy) or its name, and the port's tag under it.
void Launcher::header(float since) {
    g_alpha = ease_out(since / 1.0f);
    const ImVec2 logo_at(px(86), px(46) - px(30) * (1 - g_alpha));
    if (logo_) {
        const float h = px(132), w = h * static_cast<float>(logo_width_) / static_cast<float>(std::max(1, logo_height_));
        list_->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(logo_)), logo_at, ImVec2(logo_at.x + w, logo_at.y + h), ImVec2(0, 0), ImVec2(1, 1), fade(IM_COL32_WHITE));
        list_->AddRect(logo_at, ImVec2(logo_at.x + w, logo_at.y + h), fade(IM_COL32(255, 255, 255, 40)), 0, 0, px(1));
    } else {
        text(black_, px(64), logo_at, kInk, "UNCHARTED");
        text(heading_, px(26), ImVec2(logo_at.x + px(4), logo_at.y + px(66)), kGold, "GOLDEN ABYSS");
    }
    g_alpha = ease_out((since - 0.2f) / 0.8f);
    float tag_x = px(92);
    if (emblem_) {   // abyssport's emblem before its name
        const ImVec2 at(px(88), px(186));
        list_->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(emblem_)), at, ImVec2(at.x + px(32), at.y + px(32)), ImVec2(0, 0), ImVec2(1, 1), fade(IM_COL32_WHITE));
        tag_x += px(42);
    }
    const std::string tag = "ABYSSPORT \xC2\xB7 PC";
    for (size_t i = 0; i < tag.size(); ++i) {  // letter-spaced
        const size_t length = (static_cast<unsigned char>(tag[i]) >= 0xC0) ? 2 : 1;
        const std::string letter = tag.substr(i, length);
        text(italic_, px(16), ImVec2(tag_x, px(194)), kGold, letter.c_str());
        tag_x += measure(italic_, px(16), letter.c_str()).x + px(6);
        i += length - 1;
    }
}

// The current page in the panel on the right, sliding in.
void Launcher::panel(float now) {
    const float since = now - start_time_;
    const float enter = ease_out((now - page_since_) / 0.45f);
    page_alpha_ = g_alpha = enter * ease_out((since - 0.3f) / 0.6f);
    panel_width_ = px(600);
    const float column_width = panel_width_ + px(84);
    const float panel_x = static_cast<float>(width_) - column_width + px(50) * (1 - enter);
    const float bar_y = static_cast<float>(height_) - px(56), top = px(72);
    view_top_ = px(3);
    view_bottom_ = bar_y;
    // Another page or tab starts at its top.
    const int tab = page_ == Page::kSettings ? settings_tab_ : page_ == Page::kControls ? controls_tab_ : 0;
    if (page_ != scroll_page_ || tab != scroll_tab_) {
        scroll_page_ = page_;
        scroll_tab_ = tab;
        scroll_ = scroll_target_ = 0;
    }
    origin_ = ImVec2(panel_x + px(40), top - scroll_);
    cursor_ = 0;
    item_count_ = 0;
    item_shift_ = 0;
    list_->ChannelsSplit(2);
    row_marked_ = false;
    status_keeps_focus_ = page_ == Page::kPlay;  // a page with a big button: that one is selected first
    list_->ChannelsSetCurrent(1);
    // Rows scrolled out of the column are neither drawn nor hovered.
    ImGui::PushClipRect(ImVec2(panel_x, view_top_), ImVec2(panel_x + column_width, view_bottom_), true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_alpha);
    switch (page_) {
        case Page::kPlay: page_play(); break;
        case Page::kSettings: page_settings(); break;
        case Page::kControls: page_controls(); break;
        case Page::kAbout: page_about(); break;
        case Page::kQuit: break;
    }
    ImGui::PopStyleVar();
    origin_.y -= item_shift_;
    item_shift_ = 0;
    g_alpha = page_alpha_;

    // Scrolling: the wheel over the column, and a newly selected row brought into sight.
    const float overflow = std::max(0.0f, top + cursor_ + px(24) - view_bottom_);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (mouse.x >= panel_x && mouse.y < view_bottom_ && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
        scroll_target_ -= ImGui::GetIO().MouseWheel * px(90);
    }
    const ImGuiID nav_id = ImGui::GetCurrentContext()->NavId;
    if (row_marked_ && nav_id != scrolled_to_) {
        scrolled_to_ = nav_id;
        if (row_b_.y > view_bottom_ - px(24)) scroll_target_ = scroll_ + row_b_.y - (view_bottom_ - px(24));
        if (row_a_.y < top) {
            scroll_target_ = scroll_ - (top - row_a_.y);
            if (scroll_target_ < top) scroll_target_ = 0;  // near the top: the whole top, title included
        }
    }
    scroll_target_ = std::clamp(scroll_target_, 0.0f, overflow);
    scroll_ += (scroll_target_ - scroll_) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14);
    if (std::fabs(scroll_target_ - scroll_) < 0.5f) scroll_ = scroll_target_;
    // More above or below: the column's colour over the edge, and a thin bar for where the page is.
    if (overflow > 0) {
        const ImU32 shade = fade(IM_COL32(14, 12, 10, 240)), clear = fade(IM_COL32(14, 12, 10, 0));
        const float edge = px(48), right = panel_x + column_width;
        if (scroll_ > 1) list_->AddRectFilledMultiColor(ImVec2(panel_x, view_top_), ImVec2(right, view_top_ + edge), shade, shade, clear, clear);
        if (scroll_ < overflow - 1) list_->AddRectFilledMultiColor(ImVec2(panel_x, view_bottom_ - edge), ImVec2(right, view_bottom_), clear, clear, shade, shade);
        const float track_a = top, track_b = view_bottom_ - px(16), track = track_b - track_a;
        const float thumb = track * (view_bottom_ - top) / (view_bottom_ - top + overflow);
        const float thumb_y = track_a + (track - thumb) * (scroll_ / overflow);
        list_->AddRectFilled(ImVec2(right - px(14), track_a), ImVec2(right - px(11), track_b), fade(IM_COL32(255, 255, 255, 20)));
        list_->AddRectFilled(ImVec2(right - px(14), thumb_y), ImVec2(right - px(11), thumb_y + thumb), fade(kGold));
    }
    ImGui::PopClipRect();

    list_->ChannelsSetCurrent(0);  // under the page's widgets
    const ImVec2 pa(panel_x, 0), pb(panel_x + column_width, bar_y);
    list_->AddRectFilled(pa, pb, fade(IM_COL32(14, 12, 10, 220)));
    list_->AddLine(pa, ImVec2(pa.x, pb.y), fade(IM_COL32(255, 255, 255, 20)), px(1));
    list_->AddRectFilled(pa, ImVec2(pb.x, pa.y + px(3)), fade(kGold));
    if (row_marked_) {
        list_->PushClipRect(ImVec2(panel_x, view_top_), ImVec2(pb.x, view_bottom_), true);
        list_->AddRectFilled(row_a_, row_b_, fade(IM_COL32(255, 255, 255, 28)));
        skewed_rect(list_, row_a_, ImVec2(row_a_.x + px(7), row_b_.y), fade(IM_COL32_WHITE), fade(IM_COL32_WHITE));
        list_->PopClipRect();
    }
    list_->ChannelsMerge();
}

// One more title, row or button of the page: it fades in and rises a little, 50 ms after the one above.
void Launcher::enter_item() {
    const float delay = 0.05f * static_cast<float>(std::min(item_count_++, 10));
    const float t = ease_out((static_cast<float>(ImGui::GetTime()) - page_since_ - 0.1f - delay) / 0.35f);
    origin_.y -= item_shift_;
    item_shift_ = px(14) * (1 - t);
    origin_.y += item_shift_;
    g_alpha = page_alpha_ * t;
}

// Keys acting on the whole launcher, after the frame's widgets.
void Launcher::after_frame(float now) {
    const bool ready = game_ready_ && program_ready_;
    static const float autoplay = std::getenv("ABYSS_LAUNCHER_AUTOPLAY") ? static_cast<float>(std::atof(std::getenv("ABYSS_LAUNCHER_AUTOPLAY"))) : -1.0f;
    if (autoplay >= 0 && now - start_time_ >= autoplay) {  // test scripts press Play
        play_ = ready;
        done_ = true;
    }
    if (prepare_auto_ && !setup_thread_.joinable()) done_ = true;   // the script's Prepare has ended (setup_rows joined it)
    // ABYSS_LAUNCHER_PLAY (recomplauncher.json): Play as soon as the game is ready, and quit with it; until then (the
    // first Prepare) the launcher as usual.
    static const bool play_when_ready = std::getenv("ABYSS_LAUNCHER_PLAY") != nullptr;
    if (play_when_ready && ready && !setup_thread_.joinable()) play_ = done_ = true;
    if (ready && rebinding_ < 0 && ImGui::IsKeyPressed(ImGuiKey_GamepadStart)) play_ = true;  // Start plays from any page
    if (rebinding_ < 0 && cancel_frame_ != ImGui::GetFrameCount() && (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))) {
        if (!menu_has_focus_) {
            back_to_menu();
        } else if (page_ != Page::kPlay) {
            page_ = Page::kPlay;
            menu_cursor_ = 0;
            page_since_ = now;
        }
    }
    leave_page_on_left();
}

// The page's widgets give the keys back to the menu on the left.
void Launcher::back_to_menu() {
    menu_has_focus_ = true;
    focus_panel_ = false;
    ImGui::SetNavID(0, ImGuiNavLayer_Main, 0, ImRect());
}

// Left on the page's leftmost widget (ImGui found nothing further left): back to the menu.
void Launcher::leave_page_on_left() {
    const ImGuiID nav_id = ImGui::GetCurrentContext()->NavId;
    if (left_pending_ && nav_id == left_from_) back_to_menu();
    left_pending_ = false;
    if (!menu_has_focus_ && rebinding_ < 0 && !ImGui::IsAnyItemActive() && value_moved_frame_ != ImGui::GetFrameCount() &&
        key_pressed({ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft})) {
        left_pending_ = true;
        left_from_ = nav_id;
    }
}

// The menu on the left: one item per page, the current one on a leaning gold bar. Moving the mouse over an item opens
// its page; Quit acts when pressed. Keyboard and controller: up and down move through the items (and their pages),
// right enters the page, Enter or the confirm button acts or enters.
void Launcher::side_menu(float now, float since, const char* const* labels, const Page* targets, int count) {
    const bool ready = game_ready_ && program_ready_;
    const auto action_only = [&](int i) { return targets[i] == Page::kQuit; };
    const auto select = [&](int i) {
        menu_cursor_ = i;
        if (!action_only(i) && page_ != targets[i]) {
            page_ = targets[i];
            page_since_ = now;
            rebinding_ = -1;
        }
    };
    const auto act = [&](int i) {
        if (targets[i] == Page::kQuit) {
            done_ = true;
        } else if (targets[i] == Page::kPlay && ready) {
            play_ = true;
        } else {
            menu_has_focus_ = false;  // into the page
            focus_panel_ = true;
        }
    };
    if (menu_cursor_ >= count || (!action_only(menu_cursor_) && targets[menu_cursor_] != page_)) {
        for (int i = 0; i < count; ++i) {
            if (targets[i] == page_) menu_cursor_ = i;
        }
    }
    if (menu_has_focus_ && rebinding_ < 0) {
        if (menu_key_pressed({ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp})) {
            select((menu_cursor_ + count - 1) % count);
        } else if (menu_key_pressed({ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown})) {
            select((menu_cursor_ + 1) % count);
        } else if (key_pressed({ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_GamepadFaceDown})) {
            act(menu_cursor_);
        } else if (key_pressed({ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight}) && !action_only(menu_cursor_)) {
            menu_has_focus_ = false;
            focus_panel_ = true;
            entered_page_frame_ = ImGui::GetFrameCount();
        }
    }
    // Q and E, LB and RB: the previous and next page, from anywhere; on Settings and Controls they change the tab.
    if (rebinding_ < 0 && page_ != Page::kSettings && page_ != Page::kControls) {
        const bool previous = key_pressed({ImGuiKey_GamepadL1, ImGuiKey_Q, ImGuiKey_PageUp});
        const bool next = key_pressed({ImGuiKey_GamepadR1, ImGuiKey_E, ImGuiKey_PageDown});
        if (previous || next) {
            if (!menu_has_focus_) back_to_menu();
            int i = menu_cursor_;
            do {
                i = (i + (next ? 1 : count - 1)) % count;
            } while (action_only(i));
            select(i);
        }
    }
    for (int i = 0; i < count; ++i) {
        g_alpha = ease_out((since - 0.25f - 0.07f * static_cast<float>(i)) / 0.7f);
        const float enter = (1 - g_alpha) * -px(60);
        const ImVec2 a(px(86) + enter, px(250) + px(64) * static_cast<float>(i));
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        const bool pressed = ImGui::InvisibleButton("##menu", ImVec2(px(360), px(54)));  // the keys are handled above
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const ImVec2 mouse = ImGui::GetIO().MouseDelta;
        if (hovered && mouse.x * mouse.x + mouse.y * mouse.y > 0 && hover_item_ != i) {  // the mouse arrived here
            hover_item_ = i;
            hover_since_ = now;
            hover_armed_ = true;
        }
        if (hovered && hover_armed_ && hover_item_ == i && now - hover_since_ > 0.12f) {  // and rested a moment
            hover_armed_ = false;
            select(i);
        }
        if (!hovered && hover_item_ == i) hover_item_ = -1;
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (pressed) {
            select(i);
            act(i);
        }
        const bool current = action_only(menu_cursor_) ? menu_cursor_ == i : targets[i] == page_;
        const float on = anim(ImGui::GetID(labels[i]), current);
        const float shift = px(16) * on;
        const float bar = measure(black_, px(36), labels[i]).x + px(64);  // the label plus padding
        const ImVec2 b(a.x + bar * on + shift, a.y + px(54));
        if (on > 0.01f) {
            // Brighter while the keys move through the menu; darker once they work on the page.
            const float lit = anim(ImGui::GetID(labels[i]) + 1, menu_has_focus_);
            skewed_rect(list_, ImVec2(a.x + shift, a.y), b, mix(IM_COL32(70, 48, 10, 255), kGold, lit), mix(IM_COL32(50, 32, 6, 255), kGoldDark, lit));
            skewed_rect(list_, ImVec2(a.x - px(14) + shift, a.y), ImVec2(a.x - px(6) + shift, b.y), fade(IM_COL32_WHITE, on * (0.25f + 0.75f * lit)),
                        fade(IM_COL32_WHITE, on * (0.25f + 0.75f * lit)));
            if (lit > 0.01f) list_->AddRectFilled(ImVec2(a.x + shift, b.y + px(3)), ImVec2(b.x, b.y + px(6)), fade(IM_COL32_WHITE, lit));
            // The bar trails off to the right, and a glint runs along it every 2.4 s.
            skewed_rect(list_, ImVec2(b.x, a.y), ImVec2(b.x + px(150) * on, b.y), fade(kGoldDark, 0.6f * on), fade(kGoldDark, 0));
            const float run = ease_out(std::fmod(now / 2.4f, 1.0f) / 0.6f), glint = px(70);
            const float x = a.x + shift - glint + (b.x + px(150) - a.x - shift + glint) * run;
            const ImU32 clear = fade(IM_COL32_WHITE, 0), light = fade(IM_COL32_WHITE, 0.4f * on * (1 - run));
            list_->PushClipRect(ImVec2(a.x + shift, a.y), ImVec2(b.x + px(150) * on, b.y), true);
            list_->AddRectFilledMultiColor(ImVec2(x - glint, a.y), ImVec2(x, b.y), clear, light, light, clear);
            list_->AddRectFilledMultiColor(ImVec2(x, a.y), ImVec2(x + glint, b.y), light, clear, clear, light);
            list_->PopClipRect();
        }
        const float lit_text = anim(ImGui::GetID(labels[i]) + 2, current && menu_has_focus_);
        text(black_, px(36), ImVec2(a.x + px(22) + shift, a.y + px(7)),
             mix(mix(hovered ? kInk : kDim, IM_COL32_WHITE, on), IM_COL32(18, 14, 6, 255), lit_text), labels[i]);
    }
}

// Game, controller and version on the left; the keys that select and go back on the right.
void Launcher::bottom_bar(float since) {
    const Texts& t = T();
    const ImVec2 screen(static_cast<float>(width_), static_cast<float>(height_));
    g_alpha = ease_out((since - 0.4f) / 0.6f);
    const float bar_y = screen.y - px(56);
    list_->AddRectFilled(ImVec2(0, bar_y), screen, fade(IM_COL32(5, 4, 3, 200)));
    list_->AddLine(ImVec2(0, bar_y), ImVec2(screen.x, bar_y), fade(IM_COL32(255, 255, 255, 18)));
    float x = px(86);
    const float text_y = bar_y + px(17);
    const auto field = [&](const char* label, const char* value) {
        text(body_, px(18), ImVec2(x, text_y), kDim, label);
        x += measure(body_, px(18), label).x + px(6);
        text(body_bold_, px(18), ImVec2(x, text_y), kInk, value);
        x += measure(body_bold_, px(18), value).x + px(28);
    };
    field(t.bar_game, game_ready_ ? t.bar_game_ok : t.bar_game_none);
    field(t.bar_pad, pad_connected_ ? t.bar_pad_on : t.bar_pad_off);
    field(t.bar_version, ABYSS_VERSION);
    float right = screen.x - px(86);
    const auto hint = [&](const char* key, ImU32 colour, const char* label) {
        right -= measure(body_, px(18), label).x;
        text(body_, px(18), ImVec2(right, text_y), kDim, label);
        const float key_width = std::max(px(24), measure(body_bold_, px(15), key).x + px(12));
        right -= key_width + px(8);
        list_->AddRectFilled(ImVec2(right, text_y), ImVec2(right + key_width, text_y + px(22)), fade(colour), px(11));
        const ImVec2 key_size = measure(body_bold_, px(15), key);
        text(body_bold_, px(15), ImVec2(right + (key_width - key_size.x) / 2, text_y + (px(22) - key_size.y) / 2), IM_COL32(6, 6, 7, 255), key);
        right -= px(24);
    };
    hint("Q E", kChrome, page_ == Page::kSettings || page_ == Page::kControls ? t.tabs_hint : t.pages_hint);
    hint("Esc", IM_COL32(255, 120, 100, 255), t.back);
    hint(t.enter, kGreen, t.select);
    hint("\xE2\x86\x91\xE2\x86\x93\xE2\x86\x90\xE2\x86\x92", kChrome, t.move_hint);
}

// The window's picture into a PNG (ABYSS_LAUNCHER_SHOT): read back from the texture the frame was drawn into.
bool Launcher::save_shot(SDL_GPUTexture* texture, SDL_GPUTextureFormat format, const char* path) {
    const Uint32 size = static_cast<Uint32>(width_ * height_ * 4);
    SDL_GPUTransferBufferCreateInfo buffer_info = {SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, size, 0};
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device_, &buffer_info);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion region = {};
    region.texture = texture;
    region.w = static_cast<Uint32>(width_);
    region.h = static_cast<Uint32>(height_);
    region.d = 1;
    SDL_GPUTextureTransferInfo target = {};
    target.transfer_buffer = buffer;
    SDL_DownloadFromGPUTexture(pass, &region, &target);
    SDL_EndGPUCopyPass(pass);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    SDL_WaitForGPUFences(device_, true, &fence, 1);
    SDL_ReleaseGPUFence(device_, fence);
    unsigned char* pixels = static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device_, buffer, false));
    const bool bgra = format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    for (Uint32 i = 0; i < size; i += 4) {
        if (bgra) std::swap(pixels[i], pixels[i + 2]);
        pixels[i + 3] = 255;
    }
    const bool ok = stbi_write_png(path, width_, height_, 4, pixels, width_ * 4) != 0;
    SDL_UnmapGPUTransferBuffer(device_, buffer);
    SDL_ReleaseGPUTransferBuffer(device_, buffer);
    return ok;
}

int Launcher::run() {
    if (!start()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "abyssport", SDL_GetError(), nullptr);
        stop();
        return 1;
    }
    const char* shot = std::getenv("ABYSS_LAUNCHER_SHOT");
    if (const char* page = std::getenv("ABYSS_LAUNCHER_PAGE")) page_ = static_cast<Page>(std::clamp(std::atoi(page), 0, 3));
    if (const char* tab = std::getenv("ABYSS_LAUNCHER_TAB")) settings_tab_ = controls_tab_ = std::atoi(tab);
    if (const char* prepare = std::getenv("ABYSS_LAUNCHER_PREPARE"); prepare && tools_ready_ && std::strchr(prepare, '|')) {
        chosen_copy_.assign(prepare, std::strchr(prepare, '|'));
        chosen_pup_ = std::strchr(prepare, '|') + 1;
        start_setup();
        prepare_auto_ = true;
    }
    if (const char* item = std::getenv("ABYSS_LAUNCHER_HOW")) {  // a scene of "how to play", held, 1.6 s into its loop at the shot
        how_item_ = std::clamp(std::atoi(item), 0, 8);
        how_hold_until_ = 1e9f;
        how_since_ = 0.8f;
    }
    if (const char* device = std::getenv("ABYSS_LAUNCHER_DEVICE")) how_device_ = std::clamp(std::atoi(device), 0, 2);   // (0 keyboard and mouse, 1 DualSense, 2 Xbox)
    SDL_GPUTexture* offscreen = nullptr;
    Uint64 last = SDL_GetPerformanceCounter();
    while (!done_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) done_ = true;
            if (rebinding_ >= 0 && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {  // the key for the action being changed
                if (event.key.key != SDLK_ESCAPE) {
                    const char* name = SDL_GetKeyName(event.key.key);
                    for (const Bind& bind : kBinds) {  // a key another action had is taken from it
                        if (setting(bind.key, bind.def) == name) settings_set_str(bind.key, "");
                    }
                    settings_set_str(kBinds[rebinding_].key, name);
                    settings_save();
                }
                rebinding_ = -1;
                cancel_frame_ = ImGui::GetFrameCount() + 1;
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F11) {
                SDL_SetWindowFullscreen(window_, !(SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN));
            }
            ImGui_ImplSDL3_ProcessEvent(&event);
        }
        int count = 0;
        if (SDL_JoystickID* pads = SDL_GetGamepads(&count)) SDL_free(pads);
        pad_connected_ = count > 0;

        SDL_GetWindowSizeInPixels(window_, &width_, &height_);
        if (width_ <= 0 || height_ <= 0) {
            SDL_Delay(30);
            continue;
        }
        // Designed on 1600x900: by the height, unless the logo, the menu and the page's column (1270 design px side by
        // side) need a smaller size to fit the width.
        scale_ = std::min(static_cast<float>(width_) / 1270.0f, static_cast<float>(height_) / 900.0f);

        configure_navigation();
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const Uint64 counter = SDL_GetPerformanceCounter();
        const float dt = std::min(0.1f, static_cast<float>(counter - last) / static_cast<float>(SDL_GetPerformanceFrequency()));
        last = counter;
        const float now = static_cast<float>(ImGui::GetTime());
        frame(now, dt);
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
        SDL_GPUTexture* target = nullptr;
        const bool take_shot = shot && now - start_time_ > 2.4f;
        if (take_shot) {
            SDL_GPUTextureCreateInfo info = {};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = format_;
            info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = static_cast<Uint32>(width_);
            info.height = static_cast<Uint32>(height_);
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            target = offscreen = SDL_CreateGPUTexture(device_, &info);
        } else if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window_, &target, nullptr, nullptr)) {
            target = nullptr;
        }
        if (target) {
            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, commands);
            SDL_GPUColorTargetInfo colour = {};
            colour.texture = target;
            colour.load_op = SDL_GPU_LOADOP_CLEAR;
            colour.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &colour, 1, nullptr);
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, commands, pass);
            SDL_EndGPURenderPass(pass);
        }
        SDL_SubmitGPUCommandBuffer(commands);
        if (take_shot) {
            const bool ok = offscreen && save_shot(offscreen, format_, shot);
            if (offscreen) SDL_ReleaseGPUTexture(device_, offscreen);
            stop();
            return ok ? 0 : 1;
        }
        if (play_) {
            play_ = false;
            start_game();
            last = SDL_GetPerformanceCounter();
        }
    }
    settings_save();
    stop();
    return prepare_auto_ && !(game_ready_ && program_ready_) ? 1 : 0;
}

}  // namespace

int main(int, char**) {
    Launcher launcher;
    return launcher.run();
}
