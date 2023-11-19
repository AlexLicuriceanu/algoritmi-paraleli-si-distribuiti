# Tema 1 APD

# Nume: Alexandru LICURICEANU
# Grupa: 332CD

Am pornit de la implementarea secventiala si am organizat codul astfel:

## Main:

- Am mutat toate alocarile de memorie pentru `contour_map`, `new_image` si `grid`
in main.
- Am intializat o bariera, am pornit thread-urile si pentru fiecare am dat ca
parametru o structura care contine pointeri la image, grid, contour, etc.
- Astept ca thread-urile sa isi termine executia.
- Dupa ce toate thread-urile se termina, se scriu datele in format PPM.
- Daca imaginea a trebuit sa fie scalata, rezultatul final se va afla in
`new_image`, nu in `image`.

## Marching_in_parallel:

- In functia de thread, `marching_in_parallel`, am facut cast structurii de la void*,
am dezpachetat parametrii.
- Am luat toti pasii din algoritmul secvential si i-am agregat, calculand pentru
fiecare pas pozitia de plecare si de final pentru for-uri in functie de ID-ul thread-ului,
astfel impartind intr-un mod egal sarcinile. Paralelizarea a fost facuta pe liniile matricilor.
- Ca sincronizare, am folosit o bariera pusa intre fiecare pas al algoritmului, anume
rescaling, sampling si marching.

