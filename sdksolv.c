#include <math.h>
#include <pthread.h>
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
#ifdef __linux__
#include <strings.h>
#else
int ffs(int i) {
  static const unsigned char table[] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
  };
  unsigned int a;
  unsigned int x = i & -i;

  a = x <= 0xffff ? (x <= 0xff ? 0 : 8) : (x <= 0xffffff ? 16 : 24);

  return table[x >> a] + a;
}
#endif

#define BAR_SIZE 25

const int grpdict[SQUARES_NUM][GROUPS_PER_SQUARE] = {
  0, 9, 18, 0, 9, 19, 0, 9, 20, 1, 9, 21, 1, 9, 22, 1, 9, 23, 2, 9, 24, 2, 9, 25, 2, 9, 26, 0, 10, 18, 0, 10, 19, 0, 10, 20, 1, 10, 21, 1, 10, 22, 1, 10, 23, 2, 10, 24, 2, 10, 25, 2, 10, 26, 0, 11, 18, 0, 11, 19, 0, 11, 20, 1, 11, 21, 1, 11, 22, 1, 11, 23, 2, 11, 24, 2, 11, 25, 2, 11, 26, 3, 12, 18, 3, 12, 19, 3, 12, 20, 4, 12, 21, 4, 12, 22, 4, 12, 23, 5, 12, 24, 5, 12, 25, 5, 12, 26, 3, 13, 18, 3, 13, 19, 3, 13, 20, 4, 13, 21, 4, 13, 22, 4, 13, 23, 5, 13, 24, 5, 13, 25, 5, 13, 26, 3, 14, 18, 3, 14, 19, 3, 14, 20, 4, 14, 21, 4, 14, 22, 4, 14, 23, 5, 14, 24, 5, 14, 25, 5, 14, 26, 6, 15, 18, 6, 15, 19, 6, 15, 20, 7, 15, 21, 7, 15, 22, 7, 15, 23, 8, 15, 24, 8, 15, 25, 8, 15, 26, 6, 16, 18, 6, 16, 19, 6, 16, 20, 7, 16, 21, 7, 16, 22, 7, 16, 23, 8, 16, 24, 8, 16, 25, 8, 16, 26, 6, 17, 18, 6, 17, 19, 6, 17, 20, 7, 17, 21, 7, 17, 22, 7, 17, 23, 8, 17, 24, 8, 17, 25, 8, 17, 26
};
int grppool[GROUPS_NUM];
struct square {
  int ind, val, *gps[GROUPS_PER_SQUARE];
} sqrpool[SQUARES_NUM];
atomic_bool done;
struct config {
  int maxsol, prgdep;
  FILE *outfile, *stream, *infile;
  bool prgbar, verbose;
};

void aplval(int sqr) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    *sqrpool[sqr].gps[i] |= sqrpool[sqr].val;
}

void rlvval(int sqr) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    *sqrpool[sqr].gps[i] &= ~sqrpool[sqr].val;
}

bool testval(int sqr, int val) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    if (val & *sqrpool[sqr].gps[i])
      return false;

  return true;
}

void initgps() {
  int i, j;

  for (i = 0; i != SQUARES_NUM; ++i)
    for (j = 0; j != GROUPS_PER_SQUARE; ++j)
      sqrpool[i].gps[j] = grppool + grpdict[sqrpool[i].ind][j];
}

bool initvals(int start) {
  for (; start != SQUARES_NUM; ++start) {
    if (!testval(start, sqrpool[start].val))
      return false;

    aplval(start);
  }

  return true;
}

int rdfile(FILE *fp) {
  int c, i, n, m;

  i = n = m = 0;
  while (i != SQUARES_NUM && (c = fgetc(fp)) != EOF) {
    if (c == '0') {
      sqrpool[n].val = EMPTY_SQUARE;
      sqrpool[n++].ind = i++;
    } else if (c > '0' && c <= '9') {
      ++m;
      sqrpool[SQUARES_NUM - m].val = EMPTY_SQUARE << c - DIGIT_OFFSET;
      sqrpool[SQUARES_NUM - m].ind = i++;
    }
  }
  while (i != SQUARES_NUM) {
    sqrpool[n].val = EMPTY_SQUARE;
    sqrpool[n++].ind = i++;
  }

  fclose(fp);
  return n;
}

void wrtobuf(int *buf) {
  int i;

  for (i = 0; i != SQUARES_NUM; ++i)
    buf[sqrpool[i].ind] = ffs(sqrpool[i].val >> 1);
}

int solve(int end, int n, int *buf) {
  int i, j, d = 0;

  for (i = 0; i != n; ++i) {
    while (d != end) {
      for (j = sqrpool[d].val << 1; ; j <<= 1) {
        if (j == VALUE_END) {
          if (!d)
            return i;

          sqrpool[d].val = EMPTY_SQUARE;
          rlvval(--d);
          break;
        } else if (testval(d, j)) {
          sqrpool[d].val = j;
          aplval(d++);
          break;
        }
      }
    }

    wrtobuf(buf + i * SQUARES_NUM);
    rlvval(--d);
  }

  return i;
}

unsigned long long calcprg(int prgdep) {
  int d, tmp;
  unsigned long long prg = 0;

  for (d = 0; d != prgdep; ++d) {
    tmp = sqrpool[d].val >> 2;

    while (tmp) {
      prg += pow(GROUP_SIZE, prgdep - d - 1);
      tmp >>= 1;
    }
  }

  return prg;
}

void prtprg(unsigned long long prg, int prgdep, FILE *stream) {
  int i, tags = floor(BAR_SIZE * prg / (pow(GROUP_SIZE, prgdep)));

  fprintf(stream, "\x1b[1A\x1b[91mProgress: [");
  for (i = 0; i != tags; ++i)
    fprintf(stream, "#");
  fprintf(stream, "\x1b[0m");
  for (; i != BAR_SIZE; ++i)
    fprintf(stream, ".");
  fprintf(stream, "\x1b[91m]\x1b[0m\n");
}

void *barrtn(void *arg) {
  unsigned long long prg;
  struct config cfg = *((struct config *) arg);

  while (!done) {
    prg = calcprg(cfg.prgdep);
    prtprg(prg, cfg.prgdep, cfg.stream);
  }
}

void prtbuf(const int *buf, FILE *stream) {
  int r, c, v;

  fprintf(stream, "╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗\n║");
  for (r = 0; r != 9; ++r) {
    for (c = 0; c != 9; ++c) {
      v = buf[r * 9 + c];
      if (v)
        fprintf(stream, " %d ", v);
      else
        fprintf(stream, "   ");
      if (c % 3 == 2)
        fprintf(stream, "║");
      else
        fprintf(stream, "│");
    }

    if (r == 8)
      break;
    else if (r % 3 == 2)
      fprintf(stream, "\n╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣\n║");
    else
      fprintf(stream, "\n╟───┼───┼───╫───┼───┼───╫───┼───┼───╢\n║");
  }
  fprintf(stream, "\n╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝\n");
}

struct config prsargs(int argc, char **argv) {
  struct config cfg = {
    .maxsol = 1,
    .outfile = NULL,
    .prgbar = true,
    .prgdep = 3,
    .stream = stdout,
    .verbose = false,
    .infile = NULL
  };

  while (argc) {
    if (!strcmp("--max-solutions", *argv) || !strcmp("-m", *argv)) {
      if (argc == 1) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m No value for max-solutions specified!\x1b[0m\n\n");
        exit(EXIT_FAILURE);
      }
      cfg.maxsol = atoi(argv[1]);
      if (cfg.maxsol < 1) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m Max-solutions must be a numeric value and at least\x1b[0m 1\x1b[91m!\x1b[0m\n\n");
        exit(EXIT_FAILURE);
      }
      ++argv;
      --argc;
    } else if (!strcmp("--output", *argv) || !strcmp("-o", *argv)) {
      if (argc == 1) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m No output file specified!\x1b[0m\n\n");
        exit(EXIT_FAILURE);
      }
      cfg.outfile = fopen(argv[1], "w");
      if (cfg.outfile == NULL) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m Couldn't open file\x1b[0m %s \x1b[91mfor writing!\x1b[0m\n\n", argv[1]);
        exit(EXIT_FAILURE);
      }
      ++argv;
      --argc;
    } else if (!strcmp("--hide-progress-bar", *argv) || !strcmp("-b", *argv)) {
      cfg.prgbar = false;
    } else if (!strcmp("--progress-update-depth", *argv) || !strcmp("-d", *argv)) {
      if (argc == 1) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m No value for progress-update-depth specified!\x1b[0m\n\n");
        exit(EXIT_FAILURE);
      }
      cfg.prgdep = atoi(argv[1]);
      if (cfg.prgdep < 1 || cfg.prgdep > SQUARES_NUM) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m Progress-update-depth must be a numeric value and in range\x1b[0m 1 \x1b[91m-\x1b[0m 81\x1b[91m!\x1b[0m\n\n");
        exit(EXIT_FAILURE);
      }
      ++argv;
      --argc;
    } else if (!strcmp("--quiet", *argv) || !strcmp("-q", *argv)) {
      cfg.stream = fopen("/dev/null", "w");
      if (cfg.stream == NULL) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m Couldn't open\x1b[0m /dev/null\x1b[91mfor writing!\x1b[0m\n\n");
        exit(EXIT_FAILURE);
      }
    } else if (!strcmp("--verbose", *argv) || !strcmp("-v", *argv)) {
      cfg.verbose = true;
    } else if (!strcmp("--help", *argv) || !strcmp("-h", *argv)) {
      printf("\n\x1b[31mUsage:\x1b[91m sdksolv [\x1b[0moption...\x1b[91m] <\x1b[0mfile\x1b[91m>\n\n\x1b[31mOptions:\x1b[0m\n  -\x1b[91mm\x1b[0m, --\x1b[91mmax-solutions <\x1b[0mnumber\x1b[91m>\x1b[0m          Stop after n solutions have been found\n  -\x1b[91mo\x1b[0m, --\x1b[91moutput <\x1b[0mfile\x1b[91m>\x1b[0m                   Print solutions to file\n\n  -\x1b[91mb\x1b[0m, --\x1b[91mhide-progress-bar\x1b[0m               Disable progress bar\n  -\x1b[91md\x1b[0m, --\x1b[91mprogress-update-depth <\x1b[0mnumber\x1b[91m>\x1b[0m  Precision of progress bar\n\n  -\x1b[91mq\x1b[0m, --\x1b[91mquiet\x1b[0m                           Only print errors\n  -\x1b[91mv\x1b[0m, --\x1b[91mverbose\x1b[0m                         Print out all found solutions\n\n  -\x1b[91mh\x1b[0m, --\x1b[91mhelp\x1b[0m                            Print this page\n\n");
      exit(EXIT_SUCCESS);
    } else if(**argv == '-') {
      fprintf(stderr, "\n\x1b[31mError:\x1b[91m Unknown option\x1b[0m %s\x1b[91m!\x1b[0m\n\n\x1b[32mHint:\x1b[92m Use\x1b[0m sdksolv --help \x1b[92mfor help page.\x1b[0m\n\n", *argv);
      exit(EXIT_FAILURE);
    } else {
      if (cfg.infile != NULL) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m Multiple input files specified!\x1b[0m\n\n");
        exit(EXIT_FAILURE);
      }
      cfg.infile = fopen(*argv, "r");
      if (cfg.infile == NULL) {
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m Couldn't open file\x1b[0m %s \x1b[91mfor reading!\x1b[0m\n\n", *argv);
        exit(EXIT_FAILURE);
      }
    }

    ++argv;
    --argc;
  }

  if (cfg.infile == NULL) {
    fprintf(stderr, "\n\x1b[31mError:\x1b[91m No input file specified!\x1b[0m\n\n", *argv);
    exit(EXIT_FAILURE);
  }

  if(cfg.prgdep > 9)
    fprintf(cfg.stream, "\n\x1b[33mWarning:\x1b[93m High progress-update-depth may result in graphical errors!\x1b[0m\n\n");

  return cfg;
}

int main(int argc, char **argv) {
  struct config cfg = prsargs(argc - 1, argv + 1);
  int s, i, end = rdfile(cfg.infile), buf[cfg.maxsol * SQUARES_NUM];
  pthread_t barthrd;

  fprintf(cfg.stream, "\x1b[?25l\n\x1b[91m          < SUDOKU SOLVER >\x1b[31m\n\n  Read-in field:\x1b[0m\n");
  wrtobuf(buf);
  prtbuf(buf, cfg.stream);

  fprintf(cfg.stream, "\n\n");
  if (cfg.prgbar) {
    done = false;
    pthread_create(&barthrd, NULL, &barrtn, &cfg);
  }

  initgps();
  s = initvals(end) ? solve(end, cfg.maxsol, buf) : 0;
  if (cfg.prgbar) {
    done = true;
    pthread_join(barthrd, NULL);
    prtprg(pow(GROUP_SIZE, cfg.prgdep), cfg.prgdep, cfg.stream);
  }

  fprintf(cfg.stream, "\x1b[0m─────────────────────────────────────\n\n\x1b[91mFound \x1b[0m%d\x1b[91m solution(s)...\x1b[0m\n", s);

  if (cfg.verbose) {
    for (i = 0; i != s; ++i) {
      fprintf(cfg.stream, "\n  \x1b[31mSolution[\x1b[0m%d\x1b[31m]:\x1b[0m\n", i);
      prtbuf(buf + i * SQUARES_NUM, cfg.stream);
    }
  }

  if (cfg.outfile != NULL) {
    for (i = 0; i != s; ++i)
      prtbuf(buf + i * SQUARES_NUM, cfg.outfile);

    fclose(cfg.outfile);
  }

  fprintf(cfg.stream, "\n\x1b[91mDone.\x1b[0m\x1b[?25h\n\n");
  fclose(cfg.stream);
  return EXIT_SUCCESS;
}

