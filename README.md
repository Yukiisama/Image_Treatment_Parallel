# Image_Treatment_Parallel
http://dept-info.labri.fr/ENSEIGNEMENT/prs/feuilles-de-td/feuille-6.pdf


Université Bordeaux
Programmation Système
Licence Sciences & Technologies
Feuille 6
Accélération de calculs 2D avec des Threads
On s’intéresse à des traitements qui manipulent des images existantes, ou qui en créent des nouvelles.
On dispose d’une application qui permet d’afficher des images, de lancer des calculs dessus, et de visualiser
interactivement les évolutions de l’image à chaque itération.
Pour compiler l’application :
cd images/
make
Pour lancer l’application avec une image d’exemple (un appui sur la touche
ESC
permet de quitter l’applica-
tion) :
./prog -l images/shibuya.png
Dans le cas présent, le traitement (que l’on appellera également
noyau
) par défaut s’appelle
none
et consiste
à ne rien faire. Par défaut, c’est la variante séquentielle (
seq
) qui est invoquée.
On peut spécifier le noyau à invoyer à l’aide de l’option
-k
, et la variante à l’aide de l’option
-v
. Ainsi, on
aurait pu lancer cette première exécution comme ceci :
./prog -l images/shibuya.png -k none -v seq
Les fonctions implémentant les différentes variantes d’un noyau se trouvent généralement dans le fichier
src/<noyau>.c
. Regardez le contenu du fichier
src/none.c
, et remarquez que la fonction implémentant la
variante séquentielle s’appelle "
none_compute_seq
".
Exercice 4.1
On s’intéresse au noyau
invert
qui calcule le négatif d’une image couleur, c’est-à-dire qui
remplace chaque pixel par un pixel résultant du complément binaire de chacune de ses composantes couleur
Rouge, Vert et Bleu.
Une version séquentielle du code vous est fournie dans le fichier
src/invert.c
. Regardez comment chaque
pixel de l’image est parcouru, à l’aide des deux boucles imbriquées. La variable globale
DIM
indique à la fois
la largeur et la hauteur de l’image (qui est toujours carrée). L’accès à un pixel à la
i
eme
ligne et à la
j
eme
colonne s’effectue au travers de l’expression
cur_img(i, j)
.
Si on lance le noyau sans option particulière, l’image va clignoter de manière très rapide puisqu’à chaque
itération paire l’image retrouve son état d’origine :
./prog -l images/shibuya.png -k invert
Pour avoir le temps d’observer chaque image, il est possible de demander une pause à chaque itération :
./prog -l images/shibuya.png -k invert -d p
Enfin, pour mesurer précisément les performances du noyau
invert
sans le ralentissement dû à l’affichage,
il faut utiliser l’option
-n
(
no display
) et
-i <nb>
(nombre d’itérations souhaitées) :
./prog -l images/shibuya.png -k invert -n -i 1000
Notez le temps d’exécution total affiché.
Il s’agit désormais d’élaborer une version parallèle du noyau
invert
.
1. Écrivez une fonction
invert_compute_thread
implémentant une version parallèle dans laquelle plu-
sieurs threads calculeront chacun une bande horizontale différente de l’image.
Pour déterminer le nombre de threads à lancer, on consultera en priorité la variable d’environnement
OMP_NUM_THREADS
et, si elle n’est pas définie, on lancera autant de threads que de cœurs disponibles en
consultant le résultat de
get_nb_cores()
. Faites en sorte que chaque thread lancé puisse facilement
déterminer son numéro unique (entre
0
et le nombre total de threads lancés). Testez que le lancement
se déroule bien en vous contentant d’un
printf
dans la fonction exécutée par les threads. N’oubliez
pas d’attendre la terminaison des threads à la fin de la fonction au moyen de
pthread_join
.
Vérifiez que le programme crée bien le nombre de threads demandés :
OMP_NUM_THREAD=8 ./prog -l images/shibuya.png -k invert -v thread -i 1
2. Il faut maintenant que chaque thread calcule le numéro de la première et de la dernière ligne de
l’image qui délimitent la bande sur laquelle il va travailler. Chaque thread travaillera sur une bande
d’épaisseur
DIM
#
threads
, sauf le dernier qui aura une bande un peu plus épaisse si la division ne tombe
pas juste. Faites en sorte que chaque thread affiche l’intervalle de lignes qui lui revient.
3. Lorsque tout est correct, inspirez-vous de la double boucle dans
invert_compte_seq
pour implémenter
la version définitive. Enlevez les
printf
, et vérifiez visuellement :
./prog -l images/shibuya.png -k invert -v thread -i 1
4. Enfin, comparez les performances avec la version séquentielle :
./prog -l images/shibuya.png -k invert -v thread -n -i 1000
Que pensez vous de l’accélération obtenue (
temps sequentiel
temps parallele
) ?
Notez que dans ce cas, les threads ne sont créés qu’une seule fois, et enchainent chacun
1000
itérations de
manière indépendante. Dans le cas général, il ne sera pas toujours possible d’éviter de resynchroniser les
threads au moyen d’une barrière à la fin de chaque itération. . .
Exercice 4.2
Le noyau
invert
introduit très peu de calcul par pixel, et ses performances sont davantage
liées à la rapidité de la mémoire qu’à celle du processeur. Il est donc difficile d’obtenir des accélérations
flatteuses.
On devrait avoir davantage de chances avec le noyau
blur
, qui calcul un flou sur l’image en appliquant à un
pixel la moyenne des pixels voisins. Ouvrez le fichier
src/blur.c
et regardez comment procède la fonction
blur_compute_seq
pour calculer la nouvelle valeur de chaque pixel. Notez que le programme utilise cette
fois deux images : on lit les valeurs des pixels (provenant de l’itération précédente) qvec
cur_img(i,j)
et
on écrit la nouvelle valeur dans
next_img(i,j)
. Une fois que tous les pixels sont calculés, on inverse le rôle
des deux images en appelant
swap_images()
, puis on peut passer à l’itération suivante. . .
Essayez :
./prog -l images/shibuya.png -k blur
Comme pour le noyau
invert
, il s’agit de développer une version multi-thread.
1. Copiez-collez votre fonction
invert_compute_threads
ainsi que les fonctions annexes et variables
globales dans le fichier
src/blur.c
. Renommez la fonction principake
blur_compute_thread
.
2. Faites en sorte que chaque thread calcule le flou sur sa bande d’image. Attention, il faut cette fois une
barrière de synchronisation à la fin de chaque itération
avant
l’appel à
swap_images()
. Un thread et
un seul doit appeler
swap_images()
. Ensuite, les threads doivent à nouveau se resynchroniser avant
d’attaquer l’itération suivante.
3. Vérifiez que votre version fonctionne, puis évaluez les performances :
2
./prog -l images/shibuya.png -k blur -v thread -n -i 1000
Calculez l’accélération obtenue par rapport à la version séquentielle.
Exercice 4.3
Le noyau
mandel
affiche une représentation graphique de l’ensemble de Mandelbrot (
https:
//fr.wikipedia.org/wiki/Ensemble_de_Mandelbrot
) qui est une fractale définie comme l’ensemble des
points du plan complexe pour lesquels les termes d’une suite ont un module borné par
2
.
À chaque itération, le programme affiche une image dont les pixels ont une couleur qui dépend de la conver-
gence de lq suite associée au point complexe du plan. Entre chaque itération, un zoom est appliqué afin de
changer légèrement de point de vue.
Vous pouvez lancer le programme de la façon suivante :
./prog -s 512 -k mandel
L’option
-s 512
spécifie que l’on souhaite obtenir une image de
512
x
512
pixels.
Sans surprise, la version séquentielle
mandel_compute_seq
se trouve dans le fichier
src/mandel.c
.
1. En vous inspirant de votre travail sur le noyau
blur
, implémentez une version «
thread
» du noyau
mandel
où chaque thread créé travaillera sur une bande horizontale de l’image.
Mesurez l’accélération obtenue sur quelques itérations :
./prog -s 512 -k mandel -v thread -n -i 50
Qu’en concluez-vous ?
2. Définissez maintenant une version «
thread_cyclic
» dans laquelle les lignes de l’image sont attribuées
aux threads de manière cyclique.
Mesurez l’accélération obtenue.
3. Définissez une version «
thread_dyn
» où les lignes sont dynamiquement attribuées aux threads en
fonction de leur disponibilité. Utilisez une fonction
get_next_line()
qui sera chargée de renvoyer le
numéro de la prochaine ligne pas encore traitée
