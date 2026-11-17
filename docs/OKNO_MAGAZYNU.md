# Okno trybów magazynu / zamku

Obraz z opisem: [`docs/shots/okno-magazynu-opis.png`](shots/okno-magazynu-opis.png) (zrzut z Amigi 2,
16.11.2026, wersja 0.137, okno zamku; obraz robi `tools/annotate_inventory_mode_box.py`). Kod:
`freeserf/src/popup.cc` – `draw_resdir_box`, `handle_resdir_clk`; `game.cc` –
`set_inventory_resource_mode`, `set_inventory_serf_mode`, `update_inventories`; `serf.cc` –
`handle_serf_idle_in_stock_state`. Numery jak na obrazie.

Okno otwiera się z okna magazynu lub zamku (przewijanie). Każdy magazyn ma osobno dwa tryby:
dla **towarów** (1–4) i dla **osadników** (5–8). Zaznaczenie przy wierszu = obecny tryb.
**IN** ustawia lewy klik; **STOP i OUT tylko prawy klik** (jak w oryginale – żeby nie zmienić ich
przypadkiem).

| Nr | Nazwa | Co robi w grze |
|---|---|---|
| 1 | Obrazek: towary | tylko oznacza górną grupę |
| 2 | Towary: przyjmuj (IN) | zwykły tryb: magazyn przyjmuje towary, które nie mają innego celu, i wydaje je budynkom |
| 3 | Towary: wstrzymaj (STOP) | nie przyjmuje nowych towarów (idące do niego dostają inny cel), ale trzyma, co ma, i dalej wydaje budynkom |
| 4 | Towary: wywoź (OUT) | nie przyjmuje, a do tego wywozi swoje towary do innych magazynów – po jednym, zawsze ten o najwyższym priorytecie magazynu gracza (okno priorytetów magazynu); służy do opróżnienia magazynu |
| 5 | Obrazek: osadnicy | tylko oznacza dolną grupę |
| 6 | Osadnicy: przyjmuj (IN) | magazyn przyjmuje osadników wracających z pracy; rycerze czekający w nim szkolą się |
| 7 | Osadnicy: wstrzymaj (STOP) | nie przyjmuje nowych osadników; ci, którzy są, zostają (rycerze dalej się szkolą) |
| 8 | Osadnicy: wysyłaj (OUT) | nie przyjmuje i wysyła swoich osadników do innych magazynów (gdy kolejka przy wyjściu ma mniej niż 3); rycerze w drodze się nie szkolą |
| 9–13 | Obrońcy zamku, rycerz poz. 4 … 0 | tylko w oknie zamku: ilu obrońców zamku ma dany poziom (liczbę obrońców ustawia okno rycerzy, 16 / 19) |
| 14 | Przewiń | wraca do okna towarów zamku / magazynu |
| 15 | EXIT | zamyka okno |

Na zrzucie: zamek w trybie IN / IN, obrońcy: jeden rycerz poz. 3 i dwóch poz. 1 – przykład różnicy 4
(w oryginale silniejszy obrońca zamieniłby się ze słabszym rycerzem z magazynu zamku).
