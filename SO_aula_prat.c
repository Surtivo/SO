#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <semaphore.h>
#include <fcntl.h>

void proc1 ();
sem_t *semaphore1 ();

int main(int argc, char const *argv[])
{
    proc1();

    return 0;
}

void proc1 (){
    int teste = 90;
    sem_t *semaforo;
    sem_close(semaforo);
    sem_unlink("sem1");
    sem_destroy(semaforo);
    semaforo = semaphore1();

    int pid = fork();
    

    if(pid < 0){
        printf("\nERROR!");
        return;
    }
    else if( pid == 0){
        printf("\nESTOU NO FILHO! pid = %d", getpid());
        printf("\nVariavel do pai (herdada): %d", teste);
        sem_post(semaforo);
        /*printf("\nVariavel do pai (herdada): %d", ++teste);
        int pid2 = fork();
        printf("\nTeste");*/
    }
    else{
        sem_wait(semaforo);
        printf("\nESTOU NO PAI! pid = %d", getpid());
        teste = 100;
        printf("\nVariavel do pai: %d", teste);
        sem_close(semaforo);
        sem_unlink("sem1");
        sem_destroy(semaforo);
    }
    printf("\nEstou em ambos! pid = %d\n", getpid());
}

sem_t *semaphore1 (){
    sem_t *semaforo;
    semaforo = sem_open("sem1", O_CREAT, 066, 0);
    return semaforo;
}
