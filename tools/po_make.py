"""Writes port/po/pl.po and port/po/de.po from the translations below, in the
order of port/po/settlers.pot (run tools/po_extract.py first). A text with
no translation here stays English in the game (listed at the end).

The font of the game has capitals only; the Polish and German letters are
drawn with their marks (gfx.cc), sharp s is written SS.
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STRING = r'"((?:[^"\\]|\\.)*)"'

# (context, English): (Polish, German); \n as in the sources.
T = {
 ('', 'Off'): ('Wył.', 'Aus'),
 ('', '%d min'): ('%d min', '%d Min'),
 ('', 'Normal'): ('Normalnie', 'Normal'),
 ('', 'None'): ('Brak', 'Keine'),
 ('', '%u% of normal'): ('%u% normy', '%u% von normal'),
 ('', 'Invert scrolling'): ('Odwrotne przewijanie', 'Umgekehrt scrollen'),
 ('', 'Large numbers'): ('Duże liczby', 'Große Zahlen'),
 ('', 'Autosave'): ('Autozapis', 'Autospeichern'),
 ('', 'Stock box until occupied'): ('Magazyn do obsadzenia', 'Lager bis besetzt'),
 ('', 'Show FPS'): ('Pokaż FPS', 'FPS zeigen'),
 ('', 'Show ping in LAN game'): ('Ping w grze LAN', 'Ping im LAN-Spiel'),
 ('', 'Graphics'): ('Grafika', 'Grafik'),
 ('', 'Sounds'): ('Dźwięki', 'Klänge'),
 ('', 'Music'): ('Muzyka', 'Musik'),
 ('', 'Language'): ('Język', 'Sprache'),
 ('', 'Fast mapclick'): ('Szybka mapa', 'Schnelle Karte'),
 ('', 'An option of the original. A double click with the right mouse button on the map opens the map window.'):
   ('Opcja oryginału. Dwuklik prawym przyciskiem myszy na mapie otwiera okno mapy.',
    'Eine Option des Originals. Ein Doppelklick mit der rechten Maustaste auf die Karte öffnet das Kartenfenster.'),
 ('', 'Fast building'): ('Szybka budowa', 'Schnelles Bauen'),
 ('', 'An option of the original. A double click on a free building site opens the choice of buildings at once.'):
   ('Opcja oryginału. Dwuklik na wolnym miejscu budowy od razu otwiera wybór budynków.',
    'Eine Option des Originals. Ein Doppelklick auf einen freien Bauplatz öffnet sofort die Auswahl der Gebäude.'),
 ('', 'Sand on new maps'): ('Piasek na mapach', 'Sand auf Karten'),
 ('', 'How much desert new maps get: None, 10% of normal ... Normal. Not the missions. In a LAN game the setting of the server counts.'):
   ('Ile pustyni mają nowe mapy: Brak, 10% normy ... Normalnie. Nie dotyczy misji. W grze LAN liczy się ustawienie serwera.',
    'Wie viel Wüste neue Karten haben: Keine, 10% von normal ... Normal. Nicht die Missionen. Im LAN-Spiel zählt die Einstellung des Servers.'),
 ('', 'Rocks on new maps'): ('Skały na mapach', 'Felsen auf Karten'),
 ('', "How many brown boulders new maps get, the same way. They stand in the way of roads and buildings. In a LAN game the server's setting counts."):
   ('Ile brązowych skał mają nowe mapy, tak samo. Stoją na drodze dróg i budynków. W grze LAN liczy się ustawienie serwera.',
    'Wie viele braune Felsen neue Karten haben, ebenso. Sie stehen Wegen und Gebäuden im Weg. Im LAN-Spiel zählt die Einstellung des Servers.'),
 ('', 'Dead trees on new maps'): ('Suche drzewa', 'Tote Bäume'),
 ('', "How many dead trees new maps get, the same way. They stand in the way of buildings. In a LAN game the server's setting counts."):
   ('Ile suchych drzew mają nowe mapy, tak samo. Stoją na drodze budynków. W grze LAN liczy się ustawienie serwera.',
    'Wie viele tote Bäume neue Karten haben, ebenso. Sie stehen Gebäuden im Weg. Im LAN-Spiel zählt die Einstellung des Servers.'),
 ('', 'More kinds of trees'): ('Więcej drzew', 'Mehr Baumarten'),
 ('', 'Mirrored trees and birches among the trees of the map. Only the picture: the game stays the same.'):
   ('Odbite drzewa i brzozy wśród drzew mapy. Tylko wygląd: gra się nie zmienia.',
    'Gespiegelte Bäume und Birken zwischen den Bäumen der Karte. Nur das Bild: das Spiel bleibt gleich.'),
 ('', 'Wild boars'): ('Dziki', 'Wildschweine'),
 ('', 'A few dark brown wild boars among the trees, with their sounds. Only the picture: the game stays the same.'):
   ('Kilka ciemnobrązowych dzików wśród drzew, z ich odgłosami. Tylko wygląd: gra się nie zmienia.',
    'Ein paar dunkelbraune Wildschweine zwischen den Bäumen, mit ihren Lauten. Nur das Bild: das Spiel bleibt gleich.'),
 ('', 'Terraforming wasteland'): ('Ożywianie pustkowi', 'Ödland urbar machen'),
 ('', "Foresters plant on sand first, the sand around a new tree turns to grass for good (cacti and bones go). Lumberjacks fell dead trees first (no lumber), palms too, stonecutters break the brown boulders (no stone). New games and the game now; in a LAN game the server's setting counts."):
   ('Leśnicy sadzą najpierw na piasku, piasek wokół nowego drzewa na stałe zmienia się w trawę (kaktusy i kości znikają). Drwale najpierw ścinają suche drzewa (bez drewna), także palmy, kamieniarze rozbijają brązowe skały (bez kamienia). Nowe gry i bieżąca gra; w grze LAN liczy się ustawienie serwera.',
    'Förster pflanzen zuerst auf Sand, der Sand um einen neuen Baum wird für immer zu Gras (Kakteen und Knochen verschwinden). Holzfäller fällen zuerst tote Bäume (ohne Holz), auch Palmen, Steinmetze zerschlagen die braunen Felsen (ohne Stein). Neue Spiele und das laufende Spiel; im LAN-Spiel zählt die Einstellung des Servers.'),
 ('', 'The map moves the other way when it is dragged with the mouse. The cursor keys stay as they are.'):
   ('Mapa przesuwa się w drugą stronę przy przeciąganiu myszą. Klawisze strzałek działają jak dotąd.',
    'Die Karte bewegt sich beim Ziehen mit der Maus andersherum. Die Pfeiltasten bleiben, wie sie sind.'),
 ('', 'Numbers of 1000 and more in the boxes as 12K or .5M instead of >999.'):
   ('Liczby od 1000 w oknach jako 12T albo .5M zamiast >999.',
    'Zahlen ab 1000 in den Fenstern als 12T oder .5M statt >999.'),
 ('', 'Saves the game by itself every 3, 5, 10, 20, 30 or 60 minutes into autosave.save, unless it was paused. A click: the next time.'):
   ('Sam zapisuje grę co 3, 5, 10, 20, 30 albo 60 minut do autosave.save, chyba że była pauza. Kliknięcie: następny czas.',
    'Speichert das Spiel alle 3, 5, 10, 20, 30 oder 60 Minuten in autosave.save, außer bei Pause. Ein Klick: die nächste Zeit.'),
 ('', 'A click on a new stock opens its building box until its serf has arrived, not the box of its goods.'):
   ('Kliknięcie nowego magazynu otwiera okno budynku, dopóki nie dojdzie jego osadnik, a nie okno towarów.',
    'Ein Klick auf ein neues Lager öffnet sein Gebäudefenster, bis sein Siedler da ist, nicht das Fenster der Waren.'),
 ('', 'The frames a second in the top left corner of the screen (also the FPS Tool Type).'):
   ('Liczba klatek na sekundę w lewym górnym rogu ekranu (także Tool Type FPS).',
    'Die Bilder pro Sekunde oben links auf dem Bildschirm (auch der Tool Type FPS).'),
 ('', 'In a LAN game the time there and back of the connection in milliseconds, left of the FPS.'):
   ('W grze LAN czas odpowiedzi połączenia w milisekundach, na lewo od FPS.',
    'Im LAN-Spiel die Antwortzeit der Verbindung in Millisekunden, links von den FPS.'),
 ('', 'The language of the texts: English, Deutsch, Polski. A click: the next one, kept for the next start.'):
   ('Język napisów: English, Deutsch, Polski. Kliknięcie: następny, zapamiętany na kolejne uruchomienie.',
    'Die Sprache der Texte: English, Deutsch, Polski. Ein Klick: die nächste, gemerkt für den nächsten Start.'),
 ('', 'Font'): ('Czcionka', 'Schrift'),
 ('', 'Large games'): ('Duże gry', 'Große Spiele'),
 ('', 'Smooth'): ('Płynnie', 'Flüssig'),
 ('', 'Balanced'): ('Pośrodku', 'Ausgewogen'),
 ('', 'Full speed'): ('Pełne tempo', 'Volles Tempo'),
 ('', 'When a large game is too much for the CPU: Smooth - one game step a frame, the smoothest picture, the game slower; Balanced - steps for about a frame of time; Full speed - up to five steps a frame, the game fastest, fewer frames. Small games run at full speed anyway.'):
   ('Gdy duża gra przerasta procesor: Płynnie - jeden krok gry na klatkę, najpłynniejszy obraz, gra wolniejsza; Pośrodku - kroki przez mniej więcej czas klatki; Pełne tempo - do pięciu kroków na klatkę, gra najszybsza, mniej klatek. Małe gry i tak idą w pełnym tempie.',
    'Wenn ein großes Spiel die CPU überfordert: Flüssig - ein Spielschritt pro Bild, das flüssigste Bild, das Spiel langsamer; Ausgewogen - Schritte für etwa die Zeit eines Bildes; Volles Tempo - bis zu fünf Schritte pro Bild, das Spiel am schnellsten, weniger Bilder. Kleine Spiele laufen ohnehin mit vollem Tempo.'),
 ('', 'Starting the game'): ('Uruchamiam grę', 'Starte das Spiel'),
 ('', 'Readable'): ('Czytelna', 'Lesbar'),
 ('', 'Original'): ('Oryginalna', 'Original'),
 ('', 'The letters of the texts: Original, the font of the game; Readable, plainer letters with small letters. A click: the other one, kept for the next start.'):
   ('Litery napisów: Oryginalna, czcionka gry; Czytelna, prostsze litery, także małe. Kliknięcie: druga, zapamiętana na kolejne uruchomienie.',
    'Die Buchstaben der Texte: Original, die Schrift des Spiels; Lesbar, schlichtere Buchstaben mit Kleinbuchstaben. Ein Klick: die andere, gemerkt für den nächsten Start.'),
 ('', 'The music of the game on or off (the M key).'):
   ('Muzyka gry włączona albo wyłączona (klawisz M).',
    'Die Musik des Spiels ein oder aus (die Taste M).'),
 ('', 'The sounds of the game on or off (the S key).'):
   ('Dźwięki gry włączone albo wyłączone (klawisz S).',
    'Die Klänge des Spiels ein oder aus (die Taste S).'),
 ('', 'How loud the sounds and the music are together, 0 to 100.'):
   ('Głośność dźwięków i muzyki razem, od 0 do 100.',
    'Wie laut Klänge und Musik zusammen sind, 0 bis 100.'),
 ('', 'Music volume'): ('Głośność muzyki', 'Musiklautstärke'),
 ('', 'Effects volume'): ('Głośność efektów', 'Effektlautstärke'),
 ('', 'How loud the music is, 0 to 100 (times Volume).'):
   ('Głośność muzyki, od 0 do 100 (razy Głośność).',
    'Wie laut die Musik ist, 0 bis 100 (mal Lautstärke).'),
 ('', 'How loud the sound effects are, 0 to 100 (times Volume); a sound already playing keeps its volume.'):
   ('Głośność efektów dźwiękowych, od 0 do 100 (razy Głośność); dźwięk, który już gra, zostaje przy swojej głośności.',
    'Wie laut die Klangeffekte sind, 0 bis 100 (mal Lautstärke); ein Klang, der schon spielt, behält seine Lautstärke.'),
 ('', 'How many messages the game shows: None, Few, Most, All.'):
   ('Ile komunikatów pokazuje gra: Brak, Mało, Więcej, Wszystkie.',
    'Wie viele Meldungen das Spiel zeigt: Keine, Wenige, Meiste, Alle.'),
 ('', 'The shape of the screen, 4:3 or 16:9: it chooses the modes of Screen. A click: the other one.'):
   ('Kształt ekranu, 4:3 albo 16:9: od niego zależą tryby Ekranu. Kliknięcie: drugi.',
    'Die Form des Bildschirms, 4:3 oder 16:9: sie bestimmt die Modi von Bildschirm. Ein Klick: die andere.'),
 ('', 'The screen mode; the arrows change it at once. Kept for the next start.'):
   ('Tryb ekranu; strzałki zmieniają go od razu. Zapamiętany na kolejne uruchomienie.',
    'Der Bildschirmmodus; die Pfeile ändern ihn sofort. Gemerkt für den nächsten Start.'),
 ('', 'Sound output'): ('Wyjście dźwięku', 'Tonausgabe'),
 ('', 'Paula: the sound chip of the Amiga (audio.device), 8 bits. AHI: ahi.device in 16 bits - a sound card, or Paula through the AHI drivers (AHI prefs). A click: the other one, at once; kept for the next start. If AHI does not open, Paula stays.'):
   ('Paula: układ dźwięku Amigi (audio.device), 8 bitów. AHI: ahi.device w 16 bitach - karta dźwiękowa albo Paula przez sterowniki AHI (ustawienia AHI). Kliknięcie: drugie, od razu; zapamiętane na kolejne uruchomienie. Jeśli AHI się nie otworzy, zostaje Paula.',
    'Paula: der Klangchip der Amiga (audio.device), 8 Bit. AHI: ahi.device in 16 Bit - eine Soundkarte oder Paula über die AHI-Treiber (AHI-Einstellungen). Ein Klick: die andere, sofort; gemerkt für den nächsten Start. Öffnet AHI nicht, bleibt Paula.'),
 ('', 'Video'): ('Obraz', 'Bild'),
 ('', 'Audio'): ('Dźwięk', 'Ton'),
 ('', 'Nothing here yet.'): ('Na razie nic.', 'Noch nichts hier.'),
 ('', 'Making the map'): ('Tworzę mapę', 'Erstelle Karte'),
 ('', 'Loading the game'): ('Wczytuję grę', 'Lade das Spiel'),
 ('', 'The map is chosen'): ('Mapę wybiera się', 'Die Karte wird'),
 ('', 'in the LAN box,'): ('w oknie LAN,', 'im LAN-Fenster'),
 ('', 'after the other'): ('gdy drugi gracz', 'gewählt, wenn der'),
 ('', 'player joins.'): ('dołączy.', 'Mitspieler kommt.'),
 ('', 'Start mission'): ('Misja', 'Mission'),
 ('', 'Mission:'): ('Misja:', 'Mission:'),
 ('', 'Start tutorial'): ('Samouczek', 'Übung'),
 ('', 'Tutorial:'): ('Samouczek:', 'Übung:'),
 ('', 'New game'): ('Nowa gra', 'Neues Spiel'),
 ('', 'Mapsize:'): ('Mapa:', 'Größe:'),
 ('', 'LAN game'): ('Gra LAN', 'LAN-Spiel'),
 ('', 'Load game'): ('Wczytaj grę', 'Spiel laden'),
 ('', 'Delete %u saves?'): ('Usunąć (%u)?', 'Löschen (%u)?'),
 ('', 'Delete the save?'): ('Usunąć zapis?', 'Löschen?'),
 ('', 'YES'): ('TAK', 'JA'),
 ('', 'NO'): ('NIE', 'NEIN'),
 ('', 'DELETE'): ('USUŃ', 'LÖSCHEN'),
 ('', 'Game speed up'): ('Szybciej', 'Schneller'),
 ('', 'Game speed down'): ('Wolniej', 'Langsamer'),
 ('', 'Normal speed'): ('Normalne tempo', 'Normales Tempo'),
 ('', 'Zoom'): ('Powiększenie', 'Zoom'),
 ('', 'Wheel'): ('Kółko', 'Rad'),
 ('', 'Zoom in and out'): ('Powiększ, pomniejsz', 'Zoom ein und aus'),
 ('', 'Pause'): ('Pauza', 'Pause'),
 ('', 'Music on-off'): ('Muzyka wł.-wył.', 'Musik ein-aus'),
 ('', 'Sound on-off'): ('Dźwięk wł.-wył.', 'Klang ein-aus'),
 ('', 'Quick save'): ('Szybki zapis', 'Schnell speichern'),
 ('', 'Screenshot'): ('Zrzut ekranu', 'Bildschirmfoto'),
 ('', 'This box'): ('To okno', 'Dieses Fenster'),
 ('', 'Quit'): ('Wyjście', 'Beenden'),
 ('', 'Keys'): ('Klawisze', 'Tasten'),
 ('', 'none'): ('brak', 'keine'),
 ('', 'Server name'): ('Nazwa serwera', 'Servername'),
 ('', 'Server address'): ('Adres serwera', 'Serveradresse'),
 ('', 'Internet address'): ('Adres w internecie', 'Internetadresse'),
 ('', 'Port'): ('Port', 'Port'),
 ('', 'AI players'): ('Gracze komp.', 'KI-Spieler'),
 ('', 'Address'): ('Adres', 'Adresse'),
 ('', 'STOP SERVER'): ('ZATRZYMAJ', 'SERVER STOP'),
 ('', 'RUN SERVER'): ('URUCHOM', 'SERVER START'),
 ('', 'Go on with a saved game'): ('Dalsza gra z zapisu', 'Gespeichertes Spiel fortsetzen'),
 ('', 'USE GAME'): ('WYBIERZ', 'NEHMEN'),
 ('', 'Searching the network...'): ('Szukam w sieci...', 'Suche im Netz...'),
 ('', 'Servers found'): ('Znalezione serwery', 'Gefundene Server'),
 ('', 'None - SEARCH or type'): ('Brak - SZUKAJ albo wpisz', 'Keiner - SUCHEN oder'),
 ('', 'the address below.'): ('adres poniżej.', 'Adresse unten eingeben.'),
 ('', 'SEARCH'): ('SZUKAJ', 'SUCHEN'),
 ('', 'CONNECTING'): ('ŁĄCZĘ', 'VERBINDE'),
 ('', 'CONNECT'): ('POŁĄCZ', 'VERBINDEN'),
 ('', 'Connected. Waiting for the'): ('Połączono. Czekam, aż serwer', 'Verbunden. Warte, bis der'),
 ('', 'server to start the game.'): ('rozpocznie grę.', 'Server das Spiel startet.'),
 ('', 'Your address'): ('Twój adres', 'Deine Adresse'),
 ('', 'The game server is not ready yet.'): ('Serwer gry nie jest jeszcze gotowy.', 'Der Spielserver ist noch nicht bereit.'),
 ('', 'CHANGE'): ('ZMIEŃ', 'ÄNDERN'),
 ('', 'AVERAGE'): ('ŚREDNIA', 'MITTEL'),
 ('', 'GREAT'): ('ŚWIETNA', 'TOLL'),
 ('', 'The game did not start.'): ('Gra nie wystartowała.', 'Das Spiel startete nicht.'),
 ('', 'No network in this version.'): ('Brak sieci w tej wersji.', 'Kein Netz in dieser Version.'),
 ('', 'Type the server address.'): ('Wpisz adres serwera.', 'Serveradresse eingeben.'),
 ('', 'Connecting to '): ('Łączę z ', 'Verbinde mit '),
 ('', 'A save was not deleted.'): ('Zapis nie został usunięty.', 'Ein Spielstand blieb.'),
 ('', 'Bad settings from the server.'): ('Złe ustawienia od serwera.', 'Falsche Daten vom Server.'),
 ('', 'The saved game could not be read.'): ('Nie można wczytać zapisu.', 'Spielstand nicht lesbar.'),
 ('', 'The saved game came broken.'): ('Zapis dotarł uszkodzony.', 'Spielstand kam kaputt an.'),
 ('', 'Receiving the game... '): ('Odbieram grę... ', 'Empfange Spiel... '),
 ('', 'Choose a saved game.'): ('Wybierz zapis.', 'Spielstand wählen.'),
 ('', 'Player 2 is not a person there.'): ('Tam gracz 2 nie jest człowiekiem.', 'Spieler 2 ist dort kein Mensch.'),
 ('', 'Saved game'): ('Zapis gry', 'Spielstand'),
 ('', 'Making the map...'): ('Tworzę mapę...', 'Erstelle Karte...'),
 ('', 'No map yet.'): ('Brak mapy.', 'Noch keine Karte.'),
 ('', 'Saved'): ('Zapis', 'Stand'),
 ('', 'Map size'): ('Rozmiar', 'Größe'),
 ('', 'Server'): ('Serwer', 'Server'),
 ('', 'Player 2'): ('Gracz 2', 'Spieler 2'),
 ('', 'Response'): ('Ping', 'Antwort'),
 ('', 'SMALLER'): ('MNIEJSZA', 'KLEINER'),
 ('', 'LARGER'): ('WIĘKSZA', 'GRÖSSER'),
 ('', 'OTHER MAP'): ('INNA MAPA', 'ANDERE'),
 ('', 'NEW MAP'): ('NOWA MAPA', 'NEUE KARTE'),
 ('', 'LOAD GAME'): ('WCZYTAJ', 'LADEN'),
 ('', 'The server chooses'): ('Serwer wybiera mapę.', 'Der Server wählt die'),
 ('', 'the map. Agree on'): ('Uzgodnijcie ją', 'Karte. Einigt euch'),
 ('', 'it in the chat.'): ('na czacie.', 'im Chat.'),
 ('', 'Chat: type, Return'): ('Czat: pisz, Return', 'Chat: tippen, Return'),
 ('', 'CANCEL'): ('ANULUJ', 'ABBRUCH'),
 ('', 'START GAME'): ('START GRY', 'SPIEL START'),
 ('', 'DISCONNECT'): ('ROZŁĄCZ', 'TRENNEN'),
 ('', 'Two players on two Amigas'): ('Dwóch graczy na dwóch Amigach', 'Zwei Spieler auf zwei Amigas'),
 ('', 'in one network.'): ('w jednej sieci.', 'in einem Netz.'),
 ('', 'START SERVER'): ('ZAŁÓŻ GRĘ', 'SERVER'),
 ('', 'This Amiga sets up the game.'): ('Ta Amiga zakłada grę.', 'Diese Amiga richtet ein.'),
 ('', 'JOIN SERVER'): ('DOŁĄCZ', 'BEITRETEN'),
 ('', 'Find the server or type'): ('Znajdź serwer albo wpisz', 'Server finden oder seine'),
 ('', 'its address and connect.'): ('jego adres i połącz się.', 'Adresse eingeben.'),
 ('', 'INTERNET GAME'): ('INTERNET', 'INTERNET'),
 ('', 'Both connect to the game server,'): ('Obie Amigi łączą się z serwerem,', 'Beide verbinden sich mit dem'),
 ('', 'no router to set up.'): ('bez ustawiania routera.', 'Spielserver, ohne Router.'),
 ('', 'A game over the game server'): ('Gra przez serwer gry', 'Spiel über den Spielserver'),
 ('', 'Game server'): ('Serwer gry', 'Spielserver'),
 ('', 'not set yet'): ('jeszcze brak', 'noch keiner'),
 ('', 'Your name'): ('Twoja nazwa', 'Dein Name'),
 ('', 'Coming soon: the game server'): ('Wkrótce: serwer gry', 'Bald: der Spielserver'),
 ('', 'is being made.'): ('jest w przygotowaniu.', 'wird gebaut.'),
 ('', 'NEW GAME'): ('NOWA GRA', 'NEUES SPIEL'),
 ('', 'JOIN GAME'): ('DOŁĄCZ', 'BEITRETEN'),
 ('', 'BACK'): ('WRÓĆ', 'ZURÜCK'),
 ('', 'A player has joined.'): ('Gracz dołączył.', 'Ein Spieler ist da.'),
 ('', 'Waiting for a player...'): ('Czekam na gracza...', 'Warte auf Spieler...'),
 ('', 'RUN SERVER to be found.'): ('URUCHOM - wtedy widać grę.', 'SERVER START: sichtbar.'),
 ('', 'GAME SAVED: SYNC ERROR'): ('ZAPISANO: BŁĄD ZGODNOŚCI', 'GESPEICHERT: SYNCFEHLER'),
 ('', 'GAME NOT SAVED'): ('GRA NIE ZAPISANA', 'NICHT GESPEICHERT'),
 ('', 'RESYNC: WAITING FOR THE SERVER'): ('RESYNC: CZEKAM NA SERWER', 'RESYNC: WARTE AUF SERVER'),
 ('', 'RESYNC FAILED'): ('RESYNC NIEUDANY', 'RESYNC FEHLGESCHLAGEN'),
 ('', 'RESYNC: THE GAME GOES TO THE OTHER SIDE'): ('RESYNC: GRA IDZIE DO GRACZA 2', 'RESYNC: SPIEL GEHT RÜBER'),
 ('', 'RESYNC: THE GAME CAME BROKEN'): ('RESYNC: GRA USZKODZONA', 'RESYNC: SPIEL KAPUTT'),
 ('', 'RESYNC DONE - THE GAMES ARE THE SAME'): ('RESYNC GOTOWY - GRY ZGODNE', 'RESYNC FERTIG - SPIELE GLEICH'),
 ('', 'STATE OF THE OTHER SIDE RECEIVED'): ('STAN DRUGIEJ STRONY ODEBRANY', 'STAND DER GEGENSEITE DA'),
 ('', 'STATE OF THE OTHER SIDE BROKEN'): ('STAN DRUGIEJ STRONY USZKODZONY', 'STAND DER GEGENSEITE KAPUTT'),
 ('', 'The other player'): ('Drugi gracz', 'Der Mitspieler'),
 ('', 'The player %s ended the game.'): ('Gracz %s zakończył grę.', 'Spieler %s beendete das Spiel.'),
 ('', 'The server %s ended the game.'): ('Serwer %s zakończył grę.', 'Server %s beendete das Spiel.'),
 ('', '%s left the game.'): ('%s opuścił grę.', '%s hat das Spiel verlassen.'),
 ('', '%s ended the game.'): ('%s zakończył grę.', '%s beendete das Spiel.'),
 ('', 'The connection to %s is lost.'): ('Utracono połączenie z %s.', 'Verbindung zu %s verloren.'),
 ('', 'Game saved.'): ('Gra zapisana.', 'Spiel gespeichert.'),
 ('', 'OUT OF STEP AT '): ('ROZJAZD W KROKU ', 'ASYNCHRON BEI '),
 ('', 'LAN: NO CONNECTION'): ('LAN: BRAK POŁĄCZENIA', 'LAN: KEINE VERBINDUNG'),
 ('', 'LAN: PLAYER GONE'): ('LAN: GRACZ ODSZEDŁ', 'LAN: SPIELER WEG'),
 ('', 'LAN: OUT OF STEP '): ('LAN: ROZJAZD ', 'LAN: ASYNCHRON '),
 ('', 'LAN: SENDING THE GAME '): ('LAN: WYSYŁAM GRĘ ', 'LAN: SENDE SPIEL '),
 ('', 'LAN: WAITING FOR '): ('LAN: CZEKAM NA ', 'LAN: WARTE AUF '),
 ('', 'PLAYER'): ('GRACZA', 'SPIELER'),
 ('', 'LAN: SLOW CONNECTION '): ('LAN: WOLNE POŁĄCZENIE ', 'LAN: LANGSAM '),
 ('', 'NOT SENT - NO CONNECTION'): ('NIE WYSŁANO - BRAK POŁĄCZENIA', 'NICHT GESENDET - KEIN NETZ'),
 ('', 'You gained\na victory here'): ('Odniosłeś tu\nzwycięstwo', 'Du hast hier\neinen Sieg\nerrungen'),
 ('', 'This mine hauls\nno more raw\nmaterials'): ('Ta kopalnia\nnie wydobywa\njuż surowców', 'Dieses Bergwerk\nfördert keine\nRohstoffe mehr'),
 ('', 'You wanted me\nto call you to\nthis location'): ('Chciałeś, żebym\nwezwał cię\nw to miejsce', 'Du wolltest\nhierher gerufen\nwerden'),
 ('', 'A knight has\noccupied this\nnew building'): ('Rycerz zajął\nten nowy\nbudynek', 'Ein Ritter hat\ndieses neue\nGebäude besetzt'),
 ('', 'A new stock\nhas been built'): ('Zbudowano\nnowy magazyn', 'Ein neues\nLager wurde\ngebaut'),
 ('', 'Because of this\nenemy building\nyou lost some\nland'): ('Przez ten\nbudynek wroga\nstraciłeś\nczęść ziemi', 'Wegen dieses\nfeindlichen\nGebäudes ist\nLand verloren'),
 ('', 'Because of this\nenemy building\nyou lost some\nland and\nsome buildings'): ('Przez ten\nbudynek wroga\nstraciłeś\nczęść ziemi\ni budynków', 'Wegen dieses\nfeindlichen\nGebäudes sind\nLand und\nGebäude weg'),
 ('', 'Emergency\nprogram\nactivated'): ('Program\nawaryjny\nwłączony', 'Notprogramm\naktiviert'),
 ('', 'Emergency\nprogram\nneutralized'): ('Program\nawaryjny\nwyłączony', 'Notprogramm\naufgehoben'),
 ('', 'A geologist\nhas found gold'): ('Geolog\nznalazł złoto', 'Ein Geologe\nhat Gold\ngefunden'),
 ('', 'A geologist\nhas found iron'): ('Geolog\nznalazł żelazo', 'Ein Geologe\nhat Eisen\ngefunden'),
 ('', 'A geologist\nhas found coal'): ('Geolog\nznalazł węgiel', 'Ein Geologe\nhat Kohle\ngefunden'),
 ('', 'A geologist\nhas found stone'): ('Geolog\nznalazł kamień', 'Ein Geologe\nhat Stein\ngefunden'),
 ('', 'You wanted me\nto call you\nto this menu'): ('Chciałeś, żebym\nwezwał cię\ndo tego menu', 'Du wolltest zu\ndiesem Menü\ngerufen werden'),
 ('', '30 min. passed\nsince the last\nsaving'): ('Minęło 30 min.\nod ostatniego\nzapisu', '30 Min. seit\ndem letzten\nSpeichern'),
 ('', 'One hour passed\nsince the last\nsaving'): ('Minęła godzina\nod ostatniego\nzapisu', 'Eine Stunde\nseit dem\nletzten\nSpeichern'),
 ('', 'You wanted me\nto call you\nto this stock'): ('Chciałeś, żebym\nwezwał cię\ndo magazynu', 'Du wolltest zu\ndiesem Lager\ngerufen werden'),
 ('', 'Your settlement\nis under attack'): ('Twoja osada\njest atakowana', 'Deine Siedlung\nwird\nangegriffen'),
 ('', 'Your knights\njust lost the\nfight'): ('Twoi rycerze\nprzegrali\nwalkę', 'Deine Ritter\nhaben den\nKampf verloren'),
 ('', 'GROUND-ANALYSIS:'): ('BADANIE GRUNTU:', 'BODENANALYSE:'),
 ('knight occupation', 'Minimum'): ('Minimum', 'Minimum'),
 ('knight occupation', 'Weak'): ('Słaba', 'Schwach'),
 ('knight occupation', 'Medium'): ('Średnia', 'Mittel'),
 ('knight occupation', 'Good'): ('Dobra', 'Gut'),
 ('knight occupation', 'Full'): ('Pełna', 'Voll'),
 ('', 'Do you want\nto quit\nSettlers?'): ('Chcesz\nzakończyć\nSettlers?', 'Willst du\nSettlers\nbeenden?'),
 ('', 'Do you want\nto quit\nthis game?'): ('Chcesz\nzakończyć\ntę grę?', 'Willst du\ndieses Spiel\nbeenden?'),
 ('', 'Yes'): ('Tak', 'Ja'),
 ('', 'Do you want\nto save\nthe game?'): ('Chcesz\nzapisać\ngrę?', 'Willst du\ndas Spiel\nspeichern?'),
 ('', 'No'): ('Nie', 'Nein'),
 ('', 'The game has not\nbeen saved\nrecently.\nAre you\nsure?'): ('Gra nie była\nostatnio\nzapisana.\nNa pewno?', 'Das Spiel wurde\nlange nicht\ngespeichert.\nSicher?'),
 ('', 'Sound effects'): ('Efekty dźwięku', 'Klangeffekte'),
 ('', 'Fullscreen video'): ('Pełny ekran', 'Vollbild'),
 ('', 'Volume'): ('Głośność', 'Lautstärke'),
 ('messages', 'None'): ('Brak', 'Keine'),
 ('messages', 'Few'): ('Mało', 'Wenige'),
 ('messages', 'Most'): ('Więcej', 'Meiste'),
 ('messages', 'All'): ('Wszystkie', 'Alle'),
 ('', 'Messages'): ('Komunikaty', 'Meldungen'),
 ('', 'Aspect'): ('Proporcje', 'Seiten'),
 ('', 'Screen'): ('Ekran', 'Bildschirm'),
 ('', 'MINING\nOUTPUT:'): ('WYDOBYCIE:', 'FÖRDERUNG:'),
 ('', 'Ordered\nBuilding'): ('Zamówiony\nbudynek', 'Bestelltes\nGebäude'),
 ('', 'Defenders:'): ('Obrońcy:', 'Verteidiger:'),
 ('', 'State:'): ('Stan:', 'Status:'),
 ('', 'Transport Info:'): ('Transport:', 'Transport:'),
 ('', 'Index:'): ('Indeks:', 'Index:'),
 ('', 'Stock of\nthis building:'): ('Zapas\nbudynku:', 'Vorrat des\nGebäudes:'),
 ('', 'CONGRATULATIONS.'): ('GRATULACJE.', 'GLÜCKWUNSCH.'),
 ('', 'YOUR ENEMIES\nRESIGN. YOU\nHAVE WON THIS\nMISSION.'): ('TWOI WROGOWIE\nSIĘ PODDALI.\nWYGRAŁEŚ TĘ\nMISJĘ.', 'DEINE FEINDE\nGEBEN AUF. DU\nHAST DIESE\nMISSION\nGEWONNEN.'),
 ('', 'NEW PASSWORD:'): ('NOWE HASŁO:', 'NEUES PASSWORT:'),
 ('', 'SORRY, ONLY ONE\nOF YOUR ENEMIES\nHAS GAINED THE\nSUPERIORITY IN\nTHIS MISSION'): ('NIESTETY, JEDEN\nZ TWOICH WROGÓW\nZDOBYŁ PRZEWAGĘ\nW TEJ MISJI', 'LEIDER HAT\nEINER DEINER\nFEINDE IN\nDIESER MISSION\nGESIEGT'),
 ('', 'WELL DONE.'): ('DOBRZE.', 'GUT GEMACHT.'),
 ('', 'YOU COMPLETED\nTHIS TUTORIAL\nGAME.'): ('UKOŃCZYŁEŚ\nTEN SAMOUCZEK.', 'DU HAST DIESE\nÜBUNG\nGESCHAFFT.'),
 ('', 'THE BLUE\nSETTLERS HAVE\nGAINED THE\nSUPERIORITY'): ('NIEBIESCY\nOSADNICY\nZDOBYLI\nPRZEWAGĘ', 'DIE BLAUEN\nSIEDLER HABEN\nDIE OBERHAND\nGEWONNEN'),
 ('', 'THE RED\nSETTLERS HAVE\nGAINED THE\nSUPERIORITY'): ('CZERWONI\nOSADNICY\nZDOBYLI\nPRZEWAGĘ', 'DIE ROTEN\nSIEDLER HABEN\nDIE OBERHAND\nGEWONNEN'),
 ('', 'THE VIOLET\nSETTLERS HAVE\nGAINED THE\nSUPERIORITY'): ('FIOLETOWI\nOSADNICY\nZDOBYLI\nPRZEWAGĘ', 'DIE VIOLETTEN\nSIEDLER HABEN\nDIE OBERHAND\nGEWONNEN'),
 ('', 'THE YELLOW\nSETTLERS HAVE\nGAINED THE\nSUPERIORITY'): ('ŻÓŁCI\nOSADNICY\nZDOBYLI\nPRZEWAGĘ', 'DIE GELBEN\nSIEDLER HABEN\nDIE OBERHAND\nGEWONNEN'),
 ('', 'Build a guard hut and a guard tower and a fortress.'): ('Zbuduj wartownię, wieżę strażniczą i fortecę.', 'Baue eine Wachstube, einen Wachturm und eine Festung.'),
 ('', 'Produce five planks and five stones.'): ('Wytwórz pięć desek i pięć kamieni.', 'Stelle fünf Bretter und fünf Steine her.'),
 ('', 'Produce five fish and five meat and five bread.'): ('Wytwórz pięć ryb, pięć mięsa i pięć chlebów.', 'Stelle fünf Fische, fünf Fleisch und fünf Brote her.'),
 ('', 'Smelt five steel bars and five gold bars.'): ('Wytop pięć sztab stali i pięć sztab złota.', 'Schmelze fünf Stahlbarren und fünf Goldbarren.'),
 ('', 'Make ten weapons and ten tools.'): ('Wykuj dziesięć broni i dziesięć narzędzi.', 'Fertige zehn Waffen und zehn Werkzeuge.'),
 ('', 'Conquer all the land of the enemy.'): ('Zdobądź całą ziemię wroga.', 'Erobere das ganze Land des Feindes.'),
 ('', 'TUTORIAL %d'): ('SAMOUCZEK %d', 'ÜBUNG %d'),
 ('', 'ORDERS:'): ('ZADANIE:', 'AUFTRAG:'),
 ('', 'Demolish:'): ('Burzenie:', 'Abreißen:'),
 ('', 'Click here'): ('Kliknij tu', 'Hier klicken'),
 ('', 'if you are\nsure'): ('jeśli jesteś\npewien', 'wenn du\nsicher bist'),
 ('', 'Save Game'): ('Zapisz grę', 'Spiel speichern'),
 ('thousands', 'K'): ('T', 'T'),
 ('millions', 'M'): ('M', 'M'),
 ('ground analysis', 'Not Present'): ('Brak', 'Nichts'),
 ('ground analysis', 'Minimum'): ('Minimum', 'Minimum'),
 ('ground analysis', 'Very Few'): ('Bardzo mało', 'Sehr wenig'),
 ('ground analysis', 'Few'): ('Mało', 'Wenig'),
 ('ground analysis', 'Below Average'): ('Poniżej średniej', 'Unter Schnitt'),
 ('ground analysis', 'Average'): ('Średnio', 'Mittel'),
 ('ground analysis', 'Above Average'): ('Powyżej średniej', 'Über Schnitt'),
 ('ground analysis', 'Much'): ('Dużo', 'Viel'),
 ('ground analysis', 'Very Much'): ('Bardzo dużo', 'Sehr viel'),
 ('ground analysis', 'Perfect'): ('Doskonale', 'Perfekt'),
 ('', 'WarpTool not found:'): ('Brak programu WarpTool:', 'WarpTool nicht gefunden:'),
 ('', 'no CPU clock, no firmware.'): ('bez zegara CPU i firmware.', 'kein CPU-Takt, keine Firmware.'),
 ('', 'Accelerator'): ('Akcelerator', 'Turbokarte'),
 ('', 'Firmware'): ('Firmware', 'Firmware'),
 ('', 'CPU clock'): ('Zegar CPU', 'CPU-Takt'),
 ('', 'Raising the clock overclocks'): ('Wyższy zegar to podkręcanie', 'Höherer Takt übertaktet'),
 ('', 'the CPU - the system may hang.'): ('CPU - system może się zawiesić.', 'die CPU - System kann hängen.'),
 ('', 'MC68060 rev. 6: 50-100 MHz.'): ('MC68060 rev. 6: 50-100 MHz.', 'MC68060 Rev. 6: 50-100 MHz.'),
 ('', 'Only for a MC68060 rev. 6.'): ('Tylko dla MC68060 rev. 6.', 'Nur für MC68060 Rev. 6.'),
 ('', 'The clock changes at once.'): ('Zegar zmienia się od razu.', 'Der Takt ändert sich sofort.'),
 ('', 'Processor'): ('Procesor', 'Prozessor'),
 ('', 'yes'): ('tak', 'ja'),
 ('', 'no'): ('nie', 'nein'),
 ('', 'Card memory'): ('Pamięć karty', 'Kartenram'),
 ('', 'No Warp found.'): ('Nie znaleziono Warpa.', 'Kein Warp gefunden.'),
}


def quote(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def unescape(s):
    return s.replace('\\n', '\n').replace('\\"', '"').replace('\\\\', '\\')


def read_pot(path):
    entries = []
    ctxt = ''
    for line in open(path, encoding='utf-8'):
        if line.startswith('msgctxt '):
            ctxt = unescape(re.findall(STRING, line)[0])
        elif line.startswith('msgid '):
            msgid = unescape(re.findall(STRING, line)[0])
            if msgid:
                entries.append((ctxt, msgid))
            ctxt = ''
    return entries


def sharp_s(text):
    return text.replace('ß', 'SS')


if __name__ == '__main__':
    entries = read_pot(os.path.join(ROOT, 'port', 'po', 'settlers.pot'))
    for lang, idx, name in (('pl', 0, 'Polish'), ('de', 1, 'German')):
        missing = []
        with open(os.path.join(ROOT, 'port', 'po', lang + '.po'), 'w',
                  encoding='utf-8', newline='\n') as f:
            f.write('# %s texts of the Settlers port (Freeserf).\n' % name)
            f.write('msgid ""\nmsgstr ""\n"Content-Type: text/plain; '
                    'charset=UTF-8\\n"\n"Language: %s\\n"\n\n' % lang)
            for ctxt, msgid in entries:
                tr = T.get((ctxt, msgid))
                text = sharp_s(tr[idx]) if tr else ''
                if not tr:
                    missing.append(msgid)
                if ctxt:
                    f.write('msgctxt %s\n' % quote(ctxt))
                f.write('msgid %s\nmsgstr %s\n\n' % (quote(msgid), quote(text)))
        print('%s.po: %d texts, %d not translated %s' %
              (lang, len(entries), len(missing), missing[:10]))
    unused = [k for k in T if k not in set(entries)]
    if unused:
        print('translations no longer in the sources:', unused)
