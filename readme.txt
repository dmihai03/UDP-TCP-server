Pacuraru Mihai Dorin, 324CC, Tema 2

La comunicarea prin intermediul TCP, de fiecare data inainte de a trimite un
mesaj a carui lungime nu era cunoscuta de cealalta parte a comunicarii, trimit
lungimea mesajului care urmeaza sa fie primit.

Server

Pentru a implementa serverul am deschis 2 socketi, unul TCP si unul UDP pe care
primeam mesaje de la clientii TCP respectiv UDP.

Daca primeam un mesaj pe socketul TCP, acesta incerca sa se conecteze la
server. Pentru acest proces verificam daca deja este un client conectat cu un
ID asemanator. Daca da, trimiteam confirmare la client, iar acesta afisa un
mesaj aferent. Daca nu, adaugam clientul in vectorul de clienti conectati.

Daca primeam un mesaj de la un client UDP, tot ce fac este sa translatez
mesajul pentru a-l trimite la clientii TCP corespunzatori topicului la care
erau abonati.

Daca primeam un mesaj de la un client TCP, pe socketul deschis conexiunii,
verificam daca este un mesaj de subscribe / unsubscribe, adaugand topicul in
vectorul de topicuri specific clientului. Daca clientul inchidea conexiunea,
il setam ca si deconectat, inchideam fd-ul socketului, dar il pastrez in
vectorul de subscriberi pentru conectarile viitoare.

Client

Clientul comunica printr-un socket TCP cu serverul. Acesta pentru inceput ii
trimite serverului ID-ul cu care vrea sa se conecteze si asteapta confirmare
de la acesta.

Dupa conectare, clientul poate trimite cereri se subcribe/unsubscribe catre
server, asteptand confirmarea ca operatiunea s-a efectuat cu succes.

Acesta poate primi mesaje de la server pentru a fi afisate. In functie de tipul
de date primit clientul afiseaza un mesaj corespunzator.
