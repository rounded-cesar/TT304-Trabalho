# Escalonador de tarefas com pool de threads

Projeto prático de TT304 – Sistemas Operacionais (FT/Unicamp).

Um thread **produtor** lê tarefas de um arquivo e as coloca em uma fila de prioridade limitada. **N threads consumidores** retiram as tarefas e simulam a execução com `sleep`. Menor tempo de execução = maior prioridade; em caso de empate, vale a ordem de chegada (FIFO).

Feito em C puro, com POSIX Threads e semáforos (`mutex`, `vazio`, `cheio`). A fila é uma lista encadeada ordenada, implementada à mão, e toda a memória é alocada dinamicamente.

## Arquivos

| Arquivo | Descrição |
| --- | --- |
| `escalonador.c` | Código-fonte |
| `Makefile` | Compilação |
| `entrada.txt` | Arquivo de entrada de exemplo |

## Requisitos

Linux, `gcc` e a biblioteca POSIX Threads.

## Compilar

```
gcc -pthread escalonador.c -o escalonador
```

ou, com o Makefile:

```
make
```

A capacidade da fila (`MAX_TASKS`) é definida na compilação e vale 20 por padrão:

```
gcc -pthread -DMAX_TASKS=10 escalonador.c -o escalonador
```

## Executar

```
./escalonador <N consumidores> <arquivo de entrada>
```

Exemplo:

```
./escalonador 3 entrada.txt
```

## Arquivo de entrada

Uma tarefa por linha, com dois inteiros: `id_tarefa tempo_execucao` (em segundos).

```
1 3
2 1
3 5
```

Linhas com id negativo, tempo negativo ou tempo acima de 1.000.000 s são ignoradas, com um aviso em `stderr`.

## Saída

```
Produtor: tarefa 1 alocada e enfileirada.
Consumidor <tid>: iniciando tarefa 2 (duracao 1 s).
Consumidor <tid>: tarefa 2 concluida.
Todas as tarefas foram concluidas. Encerrando.
```

O `<tid>` é o valor de `pthread_self()`.

## Como termina

Ao acabar o arquivo, o produtor insere N tarefas especiais (`id = -1`), uma para cada consumidor. Quem retira uma delas encerra. O thread principal aguarda todos com `pthread_join` e libera a memória.

## Links

- Relatório: [inserir link]
- Vídeo de demonstração: [inserir link]
