# Tema 2 APD

# Nume: Alexandru LICURICEANU
# Grupa: 332CD

## MyDispatcher:

- Alegerea algoritmului de planificare se face prin 4 if-uri. Toate calculele pentru
alegerea nodului la care se trimite un task se fac intr-un bloc `synchronized`.


- Round Robin: Calculez index-ul hostului folosind (lastHostIndex + 1) % n,
unde `n` este numarul total de noduri host disponibile. Valoarea lui `lastHostIndex`
pleaca initial de la -1.


- Shortest Queue: Gasesc host-ul care are lungimea cozii minima, iterand toate
host-urile. Daca doua host-uri au cozi de lungimi egale, il iau pe cel cu ID mai mic.
- Functia care determina dimensiunea cozii, `getQueueSize()`, returneaza lungimea cozii
cu task-uri in asteptare + 1 (sau 0) daca exista sau nu un thread care deja ruleaza.


- Size Interval Task Assignment: Iau tipul task-ului primit, il convertesc din tip in index si
il trimit la nodul de calcul. In teorie, daca s-ar schimba ordinea din enum-ul `TaskType`,
nu ar mai functiona corect, dar am preferat aceasta metoda in locul unui if cu 3 ramuri.


- Least Work Left: Iterez toate host-urile si il gasesc pe cel care are timpul minim ramas
de rulat pe task-uri, prin functia `getWorkLeft()`, care aduna la rezultat si timpul ramas
pentru task-ul care deja ruleaza in acel nod.

## MyHost:

- Am implementat coada de task-uri folosind o coada cu prioritati, sortata descrescator dupa
prioritatea task-urilor.


- `runningTask` reprezinta task-ul care ruleaza, `running` este folosit pentru a opri functia
`run()`, iar `execute` pentru a opri/porni functia `executeTask()`.


- In metoda `run`, rulez o bucla care extrage din coada un task si ii simuleaza
executia, folosind metoda `executeTask()`.


- `executeTask()` ruleaza un task pana cand cuanta de timp a acestuia este egala cu 0, sau pana
cand executia este intrerupta de un task nou-venit cu o prioritate mai mare. Simularea executiei
se face calculand diferenta dintre momentul curent si momentul la care task-ul a intrat in
executie, folosind `System.currentTimeMillis()`. Daca boolean-ul `execute` este gasit ca `true`,
inseamna ca task-ul curent a fost preemptat si se inchide functia. In `run()`, se extrage noul
task care are prioritate mai mare, se actualizeaza `runningThread` si se apeleaza iar `executeTask()`,
insa de data asta, cu task-ul nou.


- `addTask()` adauga in coada de task-uri si apoi verifica: Daca task-ul care este in rulare, este
preemptabil, iar cel care tocmai a fost adaugat are prioritate mai mare, seteaza `execute` pe `true`,
semnaland oprirea executiei task-ului curent, pe care il adauga inapoi in coada.