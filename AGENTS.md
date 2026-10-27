# AGENTS.md — CP Sign Bridge (wydanie publiczne)

Publiczne repozytorium wydań programu SignBridge (MIT). Kod pochodzi z repozytorium `cp-signbridge`; opis programu w [README.md](README.md), historia wydań w [CHANGELOG.md](CHANGELOG.md).

## Zasady

- Bez dopisków o narzędziach i modelach w kodzie, commitach i dokumentach.
- Bramka: `bash tools/sprawdz-autorstwo.sh` (pliki i opisy commitów od 2026-10-21; wyjątki w `tools/sprawdz-autorstwo.wyjatki`).
- Żadnych sekretów, kluczy prywatnych ani danych klientów w repozytorium.

<!-- ZASADY-WSPOLNE:START -->
## Zasady wspólne Craftprojects (obowiązują zawsze)

_Kopia z `cp-ceo/firma/zasady.md` — nie edytuj w tym repo; zmiany tylko w cp-ceo._

1. **Decyzje należą do Szymona (Prezes).** Nie zgaduj jego odpowiedzi. Da się obejść → zapisz pytanie w tickecie
   (sekcja „Pytania do właściciela”), zrób resztę, idź dalej. Nie da się → zakończ ze statusem ZABLOKOWANE i opisem.
2. **Mocne zmiany tylko przez Szymona:** przepisanie dużej części kodu, zmiana koncepcji, architektury lub stosu,
   usunięcie funkcji/modułu/pomysłu. Opisujesz je jako propozycję w tickecie, nie wykonujesz.
3. **Usuwanie:** niczego nie usuwasz (pliki, dane, repo, gałęzie na GitHubie) bez zgody Szymona. Wyjątek: kod,
   który twój ticket zastępuje i który nie jest już nigdzie używany (sprawdź grep/testami, opisz w tickecie),
   oraz nieaktualne rzeczy na testowym VPS (8220).
4. **Git:** jedna gałąź = jeden ticket (`task/<PREFIKS>-<nr>-<opis>` od aktualnego `origin/main`). Bez force push,
   bez przepisywania historii. Zmiany planu (tickety, decyzje, roadmapa) idą od razu na `main` małymi commitami
   `docs(plan): …`. Na starcie pracy zawsze `git pull --ff-only`; konflikt → STOP i opis. Wyjątek dla konkretnego repo (zatwierdzony przez Szymona) jest w AGENTS.md
   tego repo, sekcja „Zasady projektu”, i ma tam pierwszeństwo.
5. **Bez śladów AI w repo:** żadnych stopek współautorstwa, identyfikatorów sesji, dopisków „wygenerowano przez…”,
   nazw modeli ani narzędzi AI w commitach, ticketach, changelogach i kodzie (wyjątek: funkcje produktu, np. moduł
   Modele AI). Pilnuje tego bramka `tools/sprawdz-autorstwo.sh` (tam, gdzie repo ją ma).
6. **Bez sekretów i prawdziwych danych klientów** w repo. Klucze prywatne i hasła tylko w 1Password — nie czytasz
   plików kluczy, nie wpisujesz haseł.
7. **Licencje:** tylko darmowe open source pozwalające sprzedawać produkt komercyjnie; każdą zależność wpisz do
   rejestru licencji repo. Narzędzia tylko do budowania mogą mieć LGPL/MS-RL/MPL — oznacz „tylko do budowy”.
8. **Nic publicznie:** serwery tylko przez Tailscale; porty Dockera tylko na adresie Tailscale albo 127.0.0.1.
   Repo zawsze prywatne.
9. **Mac Szymona:** nie otwieraj okien na jego ekranie; jeden stos
   testowy naraz (Mac ma 16 GB), po użyciu zatrzymaj kontenery.
10. **Wygląd decyduje prototyp projektanta**, logika jest prawdziwa (żadnych atrap). Jeden standard komponentów
    w całym programie (lista, górna belka, pasek kart, okna, Kontrola zmian, edytor).
11. **Status w tickecie na bieżąco** (widać go w CP Studio): start → `Status: in-progress` + „Rozpoczęto: …”;
    koniec → `Status: review` i sekcja „Wykonanie” (co zrobione, dowody, commity, czego nie zrobiono i dlaczego,
    „Przebieg: od–do, faktyczny czas pracy, maszyna, wynik”; model i effort tylko w raporcie końcowym — nigdy w repo projektu). `done` + przeniesienie do `jira/done/` po
    pozytywnej kontroli. Na każdy brak znaleziony przy kontroli — **nowy** ticket.
12. **Dowód, nie deklaracja:** zrobione = sprawdzone tam, gdzie działa (dla ERP: na 8220) i zapisane w tickecie.
    „Zrobione lokalnie” to nie zrobione.
13. **Oszczędnie z tokenami:** czytaj tylko to, co wskazuje ticket i mapa kodu; duże pliki (prototypy, logi)
    tylko fragmentami (grep + zakres linii); wyniki testów skrócone (`| tail -40`); kończysz, gdy warunek
    zamknięcia spełniony — bez „ulepszeń” spoza ticketu.
14. **Raport na koniec** wg szablonu raportu; ostatnia linia dokładnie `STATUS: ZROBIONE` albo
    `STATUS: ZABLOKOWANE — <powód>`.
<!-- ZASADY-WSPOLNE:KONIEC -->
