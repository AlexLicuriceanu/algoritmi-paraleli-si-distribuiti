# Tema 3 APD

# Nume: Alexandru LICURICEANU
# Grupa: 332CD

## Tracker:

- Pe tracker, am organizat swarm-ul in doua parti:
- O structura de forma `vector<vector<vector<int>>>` care stocheaza
date astfel: `swarm[i][j][k]`, i este index-ul unui fisier, j este
rangul unui client, iar k este index-ul segmentului detinut de
clientul j din fisierul i.
- A doua structura este un simplu vector care la index-ul i are
dimensiunea fisierului i.

- Astept ca fiecare client sa trimita tracker-ului o lista cu toate
fisierele pe care le detine si actualizez swarm-ul.
- Pe post de "OK", trimit fiecarui client dimensiunile fisierelor.
- Pornesc o bucla infinita unde astept mesaje de la clienti, diferen-
tiate prin tag-uri (cu valori alese secvential pentru fiecare
functionalitate, in timp ce rezolvam tema).

- Toate datele pentru un mesaj catre tracker vor fi puse intr-o
structura care obligatoriu contine doua int-uri: `file_index` si
`segment_index`. Ma folosesc de aceste doua valori si de tag-uri pentru
a obtine diferite functionalitati.

- Tracker-ul primeste mesaj cu tag `3`: vrea lista de peers a fisierului 
cu index `file_index` si segmentele pe care le detin.
- Tag `6`: Clientul a terminat descarcarea segmentelor pentru fisierul de
la `file_index`.
- Tag `7`: Clientul a trimis catre swarm o actualizare cu fisierele pe
care le detine.
- Tag `8`: Clientul a terminat de descarcat toate fisierele dorite. Cand
tracker-ul primeste acest mesaj, verifica daca si restul clientilor au
terminat descarcarile, iar in caz afirmativ, trimite tuturor un mesaj
ca se pot inchide, apoi se inchide si tracker-ul.


## Client:

- Citesc fisierul de intrare corespunzator.
- Trimit un vector care contine dimensiunile fisierelor detinute.
- Astept ca tracker-ul sa trimita inapoi dimensiunile tuturor fisierelor
(pe post de semnal de OK). Asta se intampla doar dupa ce toti clientii
au trimis lista cu fisierele detinute.
- Construiesc structurile pentru argumentul thread-urilor si le pornesc.

### Download:

- Initial, verific daca acest client are fisiere de descarcat, iar daca
nu are, semnalez tracker-ului ca acest client a terminat de descarcat tot.
- Daca are fisiere de descarcat, pornesc o bucla infinita, unde:
- Pentru fiecare fisier dorit, trimit tracker-ului o cerere pentru toti
peers care sunt in swarm-ul fisierului.
- Selectez un index-ul unui segment pe care nu il am.
- Caut in lista de peers obtinuta anterior un client care sa aiba segmentul
dorit, insa clientul selectat sa fie diferit de cel de la care am cerut un
segment la iteratia anterioara, daca exista.
- Fac cerere catre clientul selectat si astept sa primesc hash-ul.
- Trimit un mesaj de actualizare, cu tag `7` catre tracker, pentru hash-ul obtinut.
- Daca am toate hash-urile din fisierul curent, acesta este complet si ii
trimit un mesaj tracker-ului spunand asta.
- Scriu in fisierul corespunzator hash-urile si verific daca mai sunt alte
fisierle incomplete. Daca nu, trimit tracker-ului mesaj ca am terminat de
descarcat toate fisierele si inchid bucla infinita.

### Upload:

- Pornesc bucla infinita, astept mesajele. Comunicarea catre thread-ul de upload
se face printr-o structura care contine tot doua int-uri, `file_index` si
`segment_index`.
- Logica de upload este foarte simpla, ori primesc cerere pentru un hash, ori
mesaj de la tracker ca pot inchide.
- Cand primesc mesaj pentru hash, doar iau hash-ul din fisier si il transmit
inapoi la clientul care l-a cerut.
- Daca `file_index` si `segment_index` sunt setate ambele pe `-1`, thread-ul se
inchide (indecsii ar fi fost invalizi oricum si s-ar fi aruncat o eroare). Am
folosit acest lucru pentru a semnala, din tracker, faptul ca thread-ul de 
upload se poate opri.

## Sincronizare:

- Ca elemente de sincronizare, am folosit:
- Bariera asteapta ca structurile de date ale tracker-ului sa fie alocate,
iar tracker-ul sa fie gata sa primeasca mesaje, inainte de a porni thread-urile
pentru peers.
- Send-uri sincronizate cu `MPI_Ssend()`.
