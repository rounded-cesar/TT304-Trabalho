#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#ifndef MAX_TASKS
#define MAX_TASKS 20
#endif

#define ID_FIM -1
#define TEMPO_FIM 2000000000 // sentinela com tempo enorme: sai por ultimo
#define TEMPO_MAX 1000000 // limite pra tempo * 10 nao estourar

typedef struct Tarefa
{
    int id;
    int tempo;
    unsigned char *dados;
    struct Tarefa *prox;
} Tarefa;

typedef struct
{
    Tarefa *cabeca;
    sem_t mutex;
    sem_t vazio;
    sem_t cheio;
} Fila;

typedef struct
{
    Fila *fila;
    FILE *arquivo;
    int nConsumidores;
} ParametrosProdutor;

void down(sem_t *s)
{
    if (sem_wait(s) != 0)
    {
        perror("Erro em sem_wait");
        exit(EXIT_FAILURE);
    }
}

void up(sem_t *s)
{
    if (sem_post(s) != 0)
    {
        perror("Erro em sem_post");
        exit(EXIT_FAILURE);
    }
}

Tarefa *criaTarefa(int id, int tempo, int sentinela)
{
    Tarefa *t;
    unsigned long i, tam;

    if ((t = (Tarefa *) malloc(sizeof(Tarefa))) == NULL)
    {
        fprintf(stderr, "Problemas na alocacao da tarefa %d\n", id);
        exit(EXIT_FAILURE);
    }

    t->id = id;
    t->prox = NULL;

    if (sentinela)
    {
        t->tempo = TEMPO_FIM;
        t->dados = NULL;
    }
    else
    {
        t->tempo = tempo;
        tam = (unsigned long) tempo * 10;
        if (tam == 0)
            tam = 1;
        if ((t->dados = (unsigned char *) malloc(tam)) == NULL)
        {
            fprintf(stderr, "Problemas na alocacao do buffer da tarefa %d\n", id);
            exit(EXIT_FAILURE);
        }
        for (i = 0; i < tam; i++)
            t->dados[i] = 0;
    }
    return t;
}

void liberaTarefa(Tarefa *t)
{
    free(t->dados);
    free(t);
}

// SJB->FIFO
void insereFila(Fila *f, Tarefa *t)
{
    Tarefa **p;

    down(&f->vazio);
    down(&f->mutex);

    p = &f->cabeca;
    while (*p != NULL && (*p)->tempo <= t->tempo)
        p = &(*p)->prox;
    t->prox = *p;
    *p = t;

    up(&f->mutex);
    up(&f->cheio);
}

Tarefa *removeFila(Fila *f)
{
    Tarefa *t;

    down(&f->cheio);
    down(&f->mutex);

    t = f->cabeca;
    f->cabeca = t->prox;
    t->prox = NULL;

    up(&f->mutex);
    up(&f->vazio);

    return t;
}

void *produtor(void *arg)
{
    ParametrosProdutor *p = (ParametrosProdutor *) arg;
    int id, tempo, i;

    while (fscanf(p->arquivo, "%d %d", &id, &tempo) == 2)
    {
        if (id < 0 || tempo < 0 || tempo > TEMPO_MAX)
        {
            fprintf(stderr, "Produtor: linha ignorada (id=%d, tempo=%d invalidos).\n",
                    id, tempo);
            continue;
        }

        insereFila(p->fila, criaTarefa(id, tempo, 0));
        printf("Produtor: tarefa %d alocada e enfileirada.\n", id);
    }

    // uma sentinela pra cada consumidor saber que acabou
    for (i = 0; i < p->nConsumidores; i++)
        insereFila(p->fila, criaTarefa(ID_FIM, 0, 1));

    pthread_exit(NULL);
}

void *consumidor(void *arg)
{
    Fila *fila = (Fila *) arg;
    Tarefa *t;
    unsigned long tid = (unsigned long) pthread_self();

    while (1)
    {
        t = removeFila(fila);

        // sentinela: nao ha mais trabalho
        if (t->id < 0)
        {
            liberaTarefa(t);
            break;
        }

        printf("Consumidor %lu: iniciando tarefa %d (duracao %d s).\n",
               tid, t->id, t->tempo);
        sleep(t->tempo);
        printf("Consumidor %lu: tarefa %d concluida.\n", tid, t->id);

        liberaTarefa(t);
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    Fila *fila;
    ParametrosProdutor *parametros;
    pthread_t *idsThread;
    FILE *arquivo;
    int n, i, err;
    int criados = 0;
    int produtorCriado = 0;
    int falha = 0;

    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s <N consumidores> <arquivo de entrada>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    n = atoi(argv[1]);
    if (n <= 0)
    {
        fprintf(stderr, "N deve ser um inteiro positivo.\n");
        exit(EXIT_FAILURE);
    }

    if ((arquivo = fopen(argv[2], "r")) == NULL)
    {
        perror("Erro ao abrir o arquivo de entrada");
        exit(EXIT_FAILURE);
    }

    fila = (Fila *) malloc(sizeof(Fila));
    parametros = (ParametrosProdutor *) malloc(sizeof(ParametrosProdutor));
    idsThread = (pthread_t *) malloc(((size_t) n + 1) * sizeof(pthread_t));

    if (fila == NULL || parametros == NULL || idsThread == NULL)
    {
        fprintf(stderr, "Problemas na alocacao de memoria\n");
        exit(EXIT_FAILURE);
    }

    fila->cabeca = NULL;
    if (sem_init(&fila->mutex, 0, 1) != 0 ||
        sem_init(&fila->vazio, 0, MAX_TASKS) != 0 ||
        sem_init(&fila->cheio, 0, 0) != 0)
    {
        perror("Erro em sem_init");
        exit(EXIT_FAILURE);
    }

    parametros->fila = fila;
    parametros->arquivo = arquivo;
    parametros->nConsumidores = n;

    for (i = 1; i <= n && !falha; i++)
    {
        err = pthread_create(&idsThread[i], NULL, consumidor, (void *) fila);
        if (err != 0)
        {
            fprintf(stderr, "Erro na criacao do thread consumidor %d\n", i);
            falha = 1;
        }
        else
            criados++;
    }

    if (!falha)
    {
        err = pthread_create(&idsThread[0], NULL, produtor, (void *) parametros);
        if (err != 0)
        {
            fprintf(stderr, "Erro na criacao do thread produtor\n");
            falha = 1;
        }
        else
            produtorCriado = 1;
    }

    // sem produtor ninguem manda as sentinelas, entao mando eu
    if (falha)
        for (i = 0; i < criados; i++)
            insereFila(fila, criaTarefa(ID_FIM, 0, 1));

    for (i = 1; i <= criados; i++)
    {
        err = pthread_join(idsThread[i], NULL);
        if (err != 0)
        {
            fprintf(stderr, "Erro na juncao do thread consumidor %d\n", i);
            falha = 1;
        }
    }

    if (produtorCriado)
    {
        err = pthread_join(idsThread[0], NULL);
        if (err != 0)
        {
            fprintf(stderr, "Erro na juncao do thread produtor\n");
            falha = 1;
        }
    }

    sem_destroy(&fila->mutex);
    sem_destroy(&fila->vazio);
    sem_destroy(&fila->cheio);
    fclose(arquivo);
    free(idsThread);
    free(parametros);
    free(fila);

    if (falha)
        return EXIT_FAILURE;

    printf("Todas as tarefas foram concluidas. Encerrando.\n");
    return 0;
}