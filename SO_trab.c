#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/msg.h>

#define KEY 0x1234          //Valor hexadecimal arbitrário;
#define SIZE 1024           //Para ter 1kb de memória alocada;
#define PERMISSION 0666     //Permissão para todos os usuários de ler e escrever;
#define MSG_KEY 0x5678      //Valor hexadecimal para a chave da fila de mensagens;

#define NUM_FILHOS 4

//Estrutura de mensagem;
struct msgbuf {
    long mtype;
    char mtext[100];
};

void* thread1(void* arg){
    int dados = *((int *)arg);
    printf("Processo - %d: Pegou a variável com valor %d\n", getpid(), dados);
    free(arg);      //Liberar memória;
    pthread_exit(NULL);
    //return NULL;
}

sem_t *semaphore1 ();
sem_t *semaphore2 ();

int main (){
    int shmid;
    int *comp_var;
    
    int shmFlag;
    shmFlag = PERMISSION | IPC_CREAT;        //PERMISSION: Permite leitura e escrita. IPC_CREAT: Cria o segmento se ele ainda não existir;


    //Criar área de memória compartilahda;
    shmid = shmget(KEY, SIZE, shmFlag);     //smhid recebe o retorno dessa função. < 0 se se der erro;
    if(shmid < 0){
        perror("Erro no shmget\n");
        exit(1);                            //Valor padrão de retorno de erros;
    }

    //Anexando o segmento de memória ao processo pai;
    comp_var = (int *)shmat(shmid, NULL, 0);
    if(comp_var == (int *)-1){
        perror("Erro no shmat\n");
        exit(1);
    }

    *comp_var = 1;

    //Limpeza de semáforas na memória;
    sem_t *semaforo1;
    sem_close(semaforo1);
    sem_unlink("sem1");
    sem_destroy(semaforo1);
    semaforo1 = semaphore1();

    sem_t *semaforo2;
    sem_close(semaforo2);
    sem_unlink("sem2");
    sem_destroy(semaforo2);
    semaforo2 = semaphore2();

    //Criação da fila de mensagens;
    int msgid = msgget(MSG_KEY, IPC_CREAT | PERMISSION);
    if (msgid < 0) {
        perror("Erro ao criar fila de mensagens\n");
        exit(1);
    }

    for(int i = 0; i < NUM_FILHOS; i++){    //Para criar os filhos sequencialmente;
        pid_t pid = fork();

        if(pid < 0){
            perror("Erro ao criar filho\n");
            exit(1);
        }else if(pid == 0){
            sem_wait(semaforo1);                    //Bloqueia o filho;
            srand(time(NULL) ^ getpid());           //Utiliza o PID para evitar processos feitos muito rapidamente e que possam usar a mesma hora relógio;
            usleep((rand() % 801 + 200) * 1000);    //Timer aleatório de 200ms a 1000ms;

            //Início de Região Crítica com exclusão mútua;
            int aux;
            sem_wait(semaforo2);                    //Semáforo para sinalizar que entrou na Região Crítica;
            aux = (*comp_var);          
            (*comp_var) -= 1;                       //Filho decrementa a variável compartilhada;
            usleep((rand() % 501 + 500) * 1000);    //Timer aleatório de 500ms a 1000ms;
            sem_post(semaforo2);                    //Semáforo para sinalizar que saiu da Região Crítica;

            //Criar thread para imprimir valor;
            pthread_t thread_id;
            int *args = malloc(sizeof(int));
            *args = aux;

            if (pthread_create(&thread_id, NULL, thread1, (void *)args) != 0) {
                perror("Erro ao criar thread\n");
                exit(1);
            }

            //Esperar thread terminar;
            pthread_join(thread_id, NULL);

            struct msgbuf msg;
            msg.mtype = 1;  //Tipo da mensagem;
            snprintf(msg.mtext, sizeof(msg.mtext), "Processo filho de PID %d foi finalizado", getpid());

            if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
                perror("Erro ao enviar mensagem\n");
            }

            printf("FILHO: Processo filho com pid %d sera encerrado!\n", getpid());
            exit(0);            //Encerra o processo filho;
        }else{
            //processos do pai;
        }
    }

    //Daqui para baixo apenas o pai irá executar, pois os filhos são encerrados no seu "else if" antes que possam passar para esta parte;
    sleep(2);                           //Pai dorme por 2 segundos;
    //Libera os filhos (instruções sequenciais, liberação é simultânea);
    for (int i = 0; i < NUM_FILHOS; i++) {
        sem_post(semaforo1);
    }

    //Espera todos os filhos terminarem;
    for (int i = 0; i < NUM_FILHOS; i++) {
        struct msgbuf msg;
        if (msgrcv(msgid, &msg, sizeof(msg.mtext), 0, 0) == -1) {
            perror("Erro ao receber mensagem\n");
        } else {
            printf("PAI: %s\n", msg.mtext);
        }
    }

    //Mensagem final do pai;
    printf("Processo pai de PID %d informa que todos os filhos foram finalizados e o valor atual da memoria compartilhada é %d.\n", getpid(), *comp_var);

    //Libera a fila de mensagens;
    msgctl(msgid, IPC_RMID, NULL);


    //Libera semáforos e encerra o programa com saída 0;
    sem_close(semaforo1);
    sem_unlink("sem1");
    sem_destroy(semaforo1);

    sem_close(semaforo2);
    sem_unlink("sem2");
    sem_destroy(semaforo2);
    
    //Libera a memória;
    if(shmdt(comp_var) < 0){
        perror("Erro ao liberar variável");
        exit(1);
    }
    if(shmctl(shmid, IPC_RMID, NULL) < 0){
        perror("Erro ao libera memória");
        exit(1);
    }
    exit(0);                            //Fechou com sucesso;

    return 0;
}

sem_t *semaphore1 (){
    sem_t *semaforo;
    semaforo = sem_open("sem1", O_CREAT, 0666, 0);
    return semaforo;
}

sem_t *semaphore2 (){
    sem_t *semaforo;
    semaforo = sem_open("sem2", O_CREAT, 0666, 1);
    return semaforo;
}