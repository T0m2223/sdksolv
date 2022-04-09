#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIGIT_OFFSET 48
#define GROUP_SIZE 9
#define GROUPS_NUM 27
#define GROUPS_PER_SQUARE 3
#define SQUARES_NUM 81
#define VALUE_END 1024
#define EMPTY_SQUARE 1
#define NO_TASK 81

const int grpdict[SQUARES_NUM][GROUPS_PER_SQUARE] = {
  0, 9, 18, 0, 9, 19, 0, 9, 20, 1, 9, 21, 1, 9, 22, 1, 9, 23, 2, 9, 24, 2, 9, 25, 2, 9, 26, 0, 10, 18, 0, 10, 19, 0, 10, 20, 1, 10, 21, 1, 10, 22, 1, 10, 23, 2, 10, 24, 2, 10, 25, 2, 10, 26, 0, 11, 18, 0, 11, 19, 0, 11, 20, 1, 11, 21, 1, 11, 22, 1, 11, 23, 2, 11, 24, 2, 11, 25, 2, 11, 26, 3, 12, 18, 3, 12, 19, 3, 12, 20, 4, 12, 21, 4, 12, 22, 4, 12, 23, 5, 12, 24, 5, 12, 25, 5, 12, 26, 3, 13, 18, 3, 13, 19, 3, 13, 20, 4, 13, 21, 4, 13, 22, 4, 13, 23, 5, 13, 24, 5, 13, 25, 5, 13, 26, 3, 14, 18, 3, 14, 19, 3, 14, 20, 4, 14, 21, 4, 14, 22, 4, 14, 23, 5, 14, 24, 5, 14, 25, 5, 14, 26, 6, 15, 18, 6, 15, 19, 6, 15, 20, 7, 15, 21, 7, 15, 22, 7, 15, 23, 8, 15, 24, 8, 15, 25, 8, 15, 26, 6, 16, 18, 6, 16, 19, 6, 16, 20, 7, 16, 21, 7, 16, 22, 7, 16, 23, 8, 16, 24, 8, 16, 25, 8, 16, 26, 6, 17, 18, 6, 17, 19, 6, 17, 20, 7, 17, 21, 7, 17, 22, 7, 17, 23, 8, 17, 24, 8, 17, 25, 8, 17, 26
};

atomic_int resnum;
pthread_cond_t endcond;

int threads_num, resmax, *resbuf, mtynum, readvals[SQUARES_NUM], readinds[SQUARES_NUM], tskbdr;

pthread_mutex_t pshmtx, popmtx, waitmtx;
int *tskque, *push_queue_ptr, *pop_queue_ptr;

typedef struct environment {
  int *grppool, *valpool, **gps;
} env_t;

void apply_value(env_t env, int sqr) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    *env.gps[sqr * GROUPS_PER_SQUARE + i] |= env.valpool[sqr];
}

void relieve_value(env_t env, int sqr) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    *env.gps[sqr * GROUPS_PER_SQUARE + i] &= ~env.valpool[sqr];
}

bool test_value(env_t env, int sqr, int val) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    if (val & *env.gps[sqr * GROUPS_PER_SQUARE + i])
      return false;

  return true;
}

void init_grp_ptr(env_t env) {
  int i, j;

  for (i = 0; i != SQUARES_NUM; ++i)
    for (j = 0; j != GROUPS_PER_SQUARE; ++j)
      env.gps[i * GROUPS_PER_SQUARE + j] = env.grppool + grpdict[readinds[i]][j];
}

void copy_values(env_t env) {
  memcpy(env.valpool, readvals, SQUARES_NUM * sizeof (int));
}

void init_values(env_t env) {
  int i;

  for (i = mtynum; i != SQUARES_NUM; ++i)
    apply_value(env, i);
}

env_t create_environment() {
  env_t env;
  env.grppool = malloc(GROUPS_NUM * sizeof (int));
  env.gps = malloc(SQUARES_NUM * GROUPS_PER_SQUARE * sizeof (int *));
  env.valpool = malloc(SQUARES_NUM * sizeof (int));
  init_grp_ptr(env);
  copy_values(env);
  init_values(env);
  return env;
}

int read_file(const char *fname) {
  FILE *fp;
  int c, i, j, n, m;

  fp = fopen(fname, "r");

  if (fp == NULL) {
    perror("Error reading file");
    exit(1);
  }

  i = n = m = 0;
  while (i != SQUARES_NUM && (c = fgetc(fp)) != EOF) {
    if (c == '0') {
      readvals[n] = EMPTY_SQUARE;
      readinds[n++] = i++;
    }
    else if (c > '0' && c <= '9') {
      ++m;
      readvals[SQUARES_NUM - m] = EMPTY_SQUARE << (c - DIGIT_OFFSET);
      readinds[SQUARES_NUM - m] = i++;
    }
  }
  while (i != SQUARES_NUM) {
    readvals[n] = EMPTY_SQUARE;
    readinds[n++] = i++;
  }

  fclose(fp);
  return n;
}

void write_to_buffer(env_t env, int *buf) {
  int i;

  for (i = 0; i != SQUARES_NUM; ++i)
    buf[readinds[i]] = ffs(env.valpool[i] >> 1);
}

bool solve(env_t env) {
  int resn, i, d = tskbdr;

  while (true) {
sheeesh:
    while (d != mtynum) {
      for (i = env.valpool[d] << 1; i != VALUE_END; i <<= 1) {
        if (test_value(env, d, i)) {
          env.valpool[d] = i;
          apply_value(env, d++);
          goto sheeesh;
        }
      }
      if (d == tskbdr)
        return false;

      env.valpool[d] = EMPTY_SQUARE;
      relieve_value(env, --d);
    }
    resn = resnum++;
    if (resn < resmax) {
      write_to_buffer(env, resbuf + resn * SQUARES_NUM);
      relieve_value(env, --d);
    }
    if (resn + 1 == resmax)
      return true;
  }
}

void print_buffer(const int *buf) {
  int r, c, v;

  printf("╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗\n║");
  for (r = 0; r != 9; ++r) {
    for (c = 0; c != 9; ++c) {
      v = buf[r * 9 + c];
      if (v)
        printf(" %d ", v);
      else
        printf("   ");
      if (c % 3 == 2)
        printf("║");
      else
        printf("│");
    }

    if (r == 8)
      break;
    else if (r % 3 == 2)
      printf("\n╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣\n║");
    else
      printf("\n╟───┼───┼───╫───┼───┼───╫───┼───┼───╢\n║");
  }
  printf("\n╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝\n");
}

void reset_environment(env_t env) {
  int i;

  for (i = 0; i != mtynum; ++i) {
    if (env.valpool[i] == EMPTY_SQUARE)
      return;

    relieve_value(env, i);
  }
}

int pop_queue(env_t env) {
  int d;
  pthread_mutex_lock(&popmtx);
  if (pop_queue_ptr == push_queue_ptr)
    return NO_TASK;
  d = pop_queue_ptr[0];
  memcpy(env.valpool, pop_queue_ptr + 1, tskbdr * sizeof (int));
  pop_queue_ptr += tskbdr + 1;
  pthread_mutex_unlock(&popmtx);
  return d;
}

void push_queue(env_t env, int d, int val) {
  pthread_mutex_lock(&pshmtx);
  push_queue_ptr[0] = d + 1;
  memcpy(push_queue_ptr + 1, env.valpool, tskbdr * sizeof (int));
  push_queue_ptr[d + 1] = val;
  push_queue_ptr += tskbdr + 1;
  pthread_mutex_unlock(&pshmtx);
}

void create_queue() {
  size_t num = (1 - pow(GROUP_SIZE, tskbdr + 1)) / (1 - GROUP_SIZE);
  pop_queue_ptr = push_queue_ptr = tskque = malloc(num * (tskbdr + 1) * sizeof (int));
  push_queue_ptr[0] = 0;
  memcpy(push_queue_ptr + 1, readvals, tskbdr * sizeof (int));
  push_queue_ptr += tskbdr + 1;
}

void apply_values(env_t env, int d) {
  int i;

  for (i = 0; i != d; ++i)
    apply_value(env, i);
}

void *mt_routine(void *) {
  int d, i, o;

  env_t env;
  env = create_environment();

  while (true) {
    d = pop_queue(env);
    apply_values(env, d);
    if (d == NO_TASK)
      continue;
    while (true) {
      o = EMPTY_SQUARE;
      for (i = EMPTY_SQUARE << 1; i != VALUE_END; i <<= 1) {
        if (test_value(env, d, i)) {
          if (o == EMPTY_SQUARE)
            o = i;
          else
            push_queue(env, d, i);
        }
      }
      if (o == EMPTY_SQUARE)
        break;
      env.valpool[d] = o;
      apply_value(env, d++);
      if (d == tskbdr) {
        if (solve(env)) {
          pthread_mutex_lock(&waitmtx);
          pthread_cond_signal(&endcond);
          pthread_mutex_unlock(&waitmtx);
        }
        break;
      }
    }
    reset_environment(env);
  }
}

void parse_arguments(int argc, char **argv) {
}

int main(int argc, char **argv) {
  int n;

  if (argc == 3)
    n = atoi(argv[2]);
  else if (argc == 2)
    n = 1;
  else {
    fprintf(stderr, "Provide exactly one or two arguments!\n");
    exit(1);
  }

  if (n < 0) {
    fprintf(stderr, "Second argument must be at least 1!\n");
    exit(1);
  }

  tskbdr = 1;
  mtynum = read_file(argv[1]);
  resmax = n;
  int i;

  threads_num = 2;
  resbuf = malloc(n * SQUARES_NUM);
  printf("Read-in field:\n");
  //write_to_buffer(buf);
  //print_buffer(buf);

  create_queue();
  pthread_t thread_ids[threads_num];
  for (i = 0; i != threads_num; ++i)
    pthread_create(thread_ids + i, NULL, *mt_routine, NULL);
  pthread_cond_wait(&endcond, &waitmtx);
  printf("loool\n");
  for (i = 0; i != threads_num; ++i) {
    if (pthread_kill(thread_ids[i], SIGKILL)) {
      fprintf(stderr, "Child processes couldn't be killed\n");
      exit(1);
    }
  }

  printf("Found %d solution(s)...\n", resnum);
  for (i = 0; i != resnum; ++i) {
    printf("Solution[%d]:\n", i);
    print_buffer(resbuf + i * SQUARES_NUM);
  }

  printf("Done.\n");

  return EXIT_SUCCESS;
}

