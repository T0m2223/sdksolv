#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DIGIT_OFFSET 48
#define GROUP_SIZE 9
#define GROUPS_NUM 27
#define GROUPS_PER_SQUARE 3
#define SQUARES_NUM 81

const int grpdict[SQUARES_NUM][GROUPS_PER_SQUARE] = {
  0, 9, 18, 0, 9, 19, 0, 9, 20, 1, 9, 21, 1, 9, 22, 1, 9, 23, 2, 9, 24, 2, 9, 25, 2, 9, 26, 0, 10, 18, 0, 10, 19, 0, 10, 20, 1, 10, 21, 1, 10, 22, 1, 10, 23, 2, 10, 24, 2, 10, 25, 2, 10, 26, 0, 11, 18, 0, 11, 19, 0, 11, 20, 1, 11, 21, 1, 11, 22, 1, 11, 23, 2, 11, 24, 2, 11, 25, 2, 11, 26, 3, 12, 18, 3, 12, 19, 3, 12, 20, 4, 12, 21, 4, 12, 22, 4, 12, 23, 5, 12, 24, 5, 12, 25, 5, 12, 26, 3, 13, 18, 3, 13, 19, 3, 13, 20, 4, 13, 21, 4, 13, 22, 4, 13, 23, 5, 13, 24, 5, 13, 25, 5, 13, 26, 3, 14, 18, 3, 14, 19, 3, 14, 20, 4, 14, 21, 4, 14, 22, 4, 14, 23, 5, 14, 24, 5, 14, 25, 5, 14, 26, 6, 15, 18, 6, 15, 19, 6, 15, 20, 7, 15, 21, 7, 15, 22, 7, 15, 23, 8, 15, 24, 8, 15, 25, 8, 15, 26, 6, 16, 18, 6, 16, 19, 6, 16, 20, 7, 16, 21, 7, 16, 22, 7, 16, 23, 8, 16, 24, 8, 16, 25, 8, 16, 26, 6, 17, 18, 6, 17, 19, 6, 17, 20, 7, 17, 21, 7, 17, 22, 7, 17, 23, 8, 17, 24, 8, 17, 25, 8, 17, 26
};

bool grppool[GROUPS_NUM][GROUP_SIZE];
struct square {
  int ind, val;
  bool *gps[GROUPS_PER_SQUARE];
} sqrpool[SQUARES_NUM];

void aplval(int sqr, bool taken) {
  int i, v = sqrpool[sqr].val;

  if (v)
    for (i = 0; i != GROUPS_PER_SQUARE; ++i)
      sqrpool[sqr].gps[i][v - 1] = taken;
}

bool testval(int sqr, int val) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    if (sqrpool[sqr].gps[i][val - 1])
      return false;

  return true;
}

void initgps() {
  int i, j;

  for (i = 0; i != SQUARES_NUM; ++i)
    for (j = 0; j != GROUPS_PER_SQUARE; ++j)
      sqrpool[i].gps[j] = grppool[grpdict[sqrpool[i].ind][j]];
}

void initvals(int start) {
  for (; start != SQUARES_NUM; ++start)
    aplval(start, true);
}

int rdfile(const char *fname) {
  FILE *fp;
  int c, i, n, m;

  fp = fopen(fname, "r");

  if (fp == NULL) {
    perror("Error reading file");
    exit(1);
  }

  i = n = m = 0;
  while (i != SQUARES_NUM && (c = fgetc(fp)) != EOF) {
    if (c == '0')
      sqrpool[n++].ind = i++;
    else if (c > '0' && c <= '9') {
      ++m;
      sqrpool[SQUARES_NUM - m].val = c - DIGIT_OFFSET;
      sqrpool[SQUARES_NUM - m].ind = i++;
    }
  }
  while (i != SQUARES_NUM)
    sqrpool[n++].ind = i++;

  fclose(fp);
  return n;
}

void wrtobuf(int *buf) {
  int i;

  for (i = 0; i != SQUARES_NUM; ++i)
    buf[sqrpool[i].ind] = sqrpool[i].val;
}

int solve(int end, int n, int *buf) {
  int i, j, d = 0;

  for (i = 0; i != n; ++i) {
    while (d != end) {
      for (j = sqrpool[d].val + 1; j != GROUP_SIZE + 1; ++j) {
        if (testval(d, j)) {
          sqrpool[d].val = j;
          aplval(d++, true);
          break;
        }
      }

      if (!d)
        return i;
      else if (j == GROUP_SIZE + 1) {
        sqrpool[d].val = 0;
        aplval(--d, false);
      }
    }

    wrtobuf(buf + i * SQUARES_NUM);
    aplval(--d, false);
  }

  return i;
}

void prtbuf(const int *buf) {
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

  if (n < 1) {
    fprintf(stderr, "Second argument must be at least 1!\n");
    exit(1);
  }

  int end = rdfile(argv[1]);
  int buf[n * SQUARES_NUM];
  int s, i;

  printf("Read-in field:\n");
  wrtobuf(buf);
  prtbuf(buf);

  initgps();
  initvals(end);
  s = solve(end, n, buf);

  printf("Found %d solution(s)...\n", s);
  for (i = 0; i != s; ++i) {
    printf("Solution[%d]:\n", i);
    prtbuf(buf + i * SQUARES_NUM);
  }

  printf("Done.\n");

  return EXIT_SUCCESS;
}

