# Okno ustawień rycerzy (Knight settings)

Obraz z opisem: [`docs/shots/okno-rycerzy-opis.png`](shots/okno-rycerzy-opis.png) (zrzut z Amigi 2,
16.11.2026, wersja 0.137). Kod: `freeserf/src/popup.cc` – `draw_sett_8_box`, `handle_sett_8_click`.
Numery są takie same jak na obrazie; można o nie pytać („co robi 15?”).

| Nr | Nazwa | Co robi w grze |
|---|---|---|
| 1 | Osadnik (obrazek) | lewy koniec suwaka 2: tam mniej rycerzy |
| 2 | **Suwak: osadnicy → rycerze** (Serf to knight rate) | jaka część nowych osadników ma zostać rycerzami. Co 10–30 s (ustawienie „rozmnażanie” gry) w zamku / magazynie powstaje osadnik; suwak dodaje się do licznika 16-bitowego, przy przepełnieniu jeden z nowych zostaje rycerzem (najwyżej 2 w kolejce). Domyślnie 20000 z 65535 ≈ 31%. Rycerz powstaje tylko, gdy w magazynie jest miecz i tarcza |
| 3 | Rycerz (obrazek) | prawy koniec suwaka 2: tam więcej rycerzy |
| 4–7 | **Szkól 1 / 5 / 20 / 100** (Train knights) | od razu zamienia tylu bezczynnych osadników z magazynów w rycerzy poziomu 0; każdy zużywa miecz i tarczę. Uwaga: w porcie (Freeserf) każdy z tych przycisków zamienia **wszystkich** możliwych, w oryginale Amigi – najwyżej tylu, ile na przycisku (`docs/ZGODNOSC.md`, różnica 3) |
| 8 | Osadnik + tarcza + miecz (obrazek) | z czego powstaje rycerz |
| 9 | **Gotowi do zamiany** | ilu osadników można zamienić teraz: suma po magazynach z min(miecze, tarcze, wolni osadnicy) |
| 10 | **Morale rycerzy %** (Knight morale) | 100% = 4096. Liczone z udziału złota gracza w całym złocie mapy (1024 + współczynnik · złoto / całe złoto) i z zamków: bez zamku −1023, za każdy zdobyty zamek +1024. Działa w walce **na cudzej ziemi**: siła rycerza = 2^poziom · morale (na własnej ziemi zawsze 100%) |
| 11 | **Złoto** (Gold deposited) | sztabki złota w magazynach i strażnicach (chata do 2, wieża 4, twierdza 8) – to z nich liczy się morale |
| 12 | Słaby / silny rycerz (obrazki) | ozdoba przy morale, kliknięcie nic nie robi |
| 13 | **Atak: najsłabsi** | przy ataku z każdego budynku idą najpierw rycerze najniższego poziomu (silniejsi zostają w obronie). Zaznaczenie ✓ pokazuje wybrany tryb |
| 14 | **Atak: najsilniejsi** | przy ataku idą najpierw najsilniejsi |
| 15 | **Wymiana rycerzy** (Cycle knights) | przez ok. 24 s gry budynki wojskowe odsyłają rycerzy do magazynów, a na ich miejsce przychodzą lepiej wyszkoleni z magazynów (najpierw spada wymagany poziom, potem budynki biorą najlepszych) |
| 16 | **Mniej obrońców zamku** (−) | ilu rycerzy ma stać w zamku: −1, najmniej 1 |
| 17 | Obrońcy zamku: chciani | ustawiona liczba (domyślnie 3) |
| 18 | Obrońcy zamku: są | ilu rycerzy jest teraz w zamku |
| 19 | **Więcej obrońców zamku** (+) | +1, najwięcej 99; przytrzymanie powtarza |
| 20 | EXIT | wraca do okna wyboru ustawień |

Związane zasady (sprawdzone z oryginałem Amigi, `docs/ZGODNOSC.md`): szkolenie rycerzy w zamku
i magazynie 4× szybsze niż w wieży i 16× niż w chacie; walka: napastnik wygrywa z szansą
siła_n / (siła_n + siła_o).
