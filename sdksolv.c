#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIGIT_OFFSET '0'
#define GROUP_SIZE 9
#define GROUPS_NUM 27
#define GROUPS_PER_SQUARE 3
#define SQUARES_NUM 81

#define VALUE_END 1024
#define EMPTY_SQUARE 1

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
  FILE *outfile, *infile;
  bool prgbar, quiet, verbose, color;
};

void aplyval(int sqr) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    *sqrpool[sqr].gps[i] |= sqrpool[sqr].val;
}

void relvval(int sqr) {
  int i;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    *sqrpool[sqr].gps[i] &= ~sqrpool[sqr].val;
}

bool testval(int sqr, int val) {
  int i, acc = 0;

  for (i = 0; i != GROUPS_PER_SQUARE; ++i)
    acc |= *sqrpool[sqr].gps[i];

  return !(acc & val);
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

    aplyval(start);
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
      sqrpool[SQUARES_NUM - m].val = EMPTY_SQUARE << (c - DIGIT_OFFSET);
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
    buf[sqrpool[i].ind] = __builtin_ctz(sqrpool[i].val);
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
          relvval(--d);
          break;
        } else if (testval(d, j)) {
          sqrpool[d].val = j;
          aplyval(d++);
          break;
        }
      }
    }

    wrtobuf(buf + i * SQUARES_NUM);
    relvval(--d);
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

void prtprg(unsigned long long prg, int prgdep) {
  int i, tags = floor(BAR_SIZE * prg / (pow(GROUP_SIZE, prgdep)));

  fputs("\x1b[1A\x1b[91mProgress: [", stdout);
  for (i = 0; i != tags; ++i)
    fputc('#', stdout);
  fputs("\x1b[0m", stdout);
  for (; i != BAR_SIZE; ++i)
    fputc('.', stdout);
  fputs("\x1b[91m]\x1b[0m\n", stdout);
}

void *barrtn(void *arg) {
  struct config cfg = *((struct config *) arg);
  unsigned long long prg;

  while (!done) {
    prg = calcprg(cfg.prgdep);
    prtprg(prg, cfg.prgdep);
  }

  return NULL;
}

void prtbuf(const int *buf, FILE *fp) {
  int r, c, v;

  fputs("╔═══╤═══╤═══╦═══╤═══╤═══╦═══╤═══╤═══╗\n║", fp);
  for (r = 0; r != 9; ++r) {
    for (c = 0; c != 9; ++c) {
      v = buf[r * 9 + c];
      if (v) {
        fputc(' ', fp);
        fputc(v + DIGIT_OFFSET, fp);
      } else
        fputs("  ", fp);
      if (c % 3 == 2)
        fputs(" ║", fp);
      else
        fputs(" │", fp);
    }

    if (r == 8)
      break;
    else if (r % 3 == 2)
      fputs("\n╠═══╪═══╪═══╬═══╪═══╪═══╬═══╪═══╪═══╣\n║", fp);
    else
      fputs("\n╟───┼───┼───╫───┼───┼───╫───┼───┼───╢\n║", fp);
  }
  fputs("\n╚═══╧═══╧═══╩═══╧═══╧═══╩═══╧═══╧═══╝\n", fp);
}

struct config prsargs(int argc, char **argv) {
  int arg = 1;
  struct config cfg = {
    .maxsol = 1,
    .outfile = NULL,
    .prgbar = false,
    .prgdep = 3,
    .quiet = false,
    .verbose = false,
    .color = false,
    .infile = NULL
  };

  while (arg != argc) {
    if (!strcmp("--max-solutions", argv[arg]) || !strcmp("-m", argv[arg])) {
      if (++arg == argc) {
        if (cfg.color)
          fputs("\n\x1b[31mError:\x1b[91m No value for max-solutions specified!\x1b[0m\n\n", stderr);
        else
          fputs("\nError: No value for max-solutions specified!\n\n", stderr);
        exit(EXIT_FAILURE);
      }
      cfg.maxsol = atoi(argv[arg]);
      if (cfg.maxsol < 1) {
        if (cfg.color)
          fputs("\n\x1b[31mError:\x1b[91m Max-solutions must be a numeric value and at least\x1b[0m 1\x1b[91m!\x1b[0m\n\n", stderr);
        else
          fputs("\nError: Max-solutions must be a numeric value and at least 1!\n\n", stderr);
        exit(EXIT_FAILURE);
      }
    } else if (!strcmp("--output", argv[arg]) || !strcmp("-o", argv[arg])) {
      if (++arg == argc) {
        if (cfg.color)
          fputs("\n\x1b[31mError:\x1b[91m No output file specified!\x1b[0m\n\n", stderr);
        else
          fputs("\nError: No output file specified!\n\n", stderr);
        exit(EXIT_FAILURE);
      }
      cfg.outfile = fopen(argv[arg], "w");
      if (!cfg.outfile) {
        if (cfg.color)
          fprintf(stderr, "\n\x1b[31mError:\x1b[91m Couldn't open file\x1b[0m %s \x1b[91mfor writing!\x1b[0m\n\n", argv[arg]);
        else
          fprintf(stderr, "\nError: Couldn't open file %s for writing!\n\n", argv[arg]);
        exit(EXIT_FAILURE);
      }
    } else if (!strcmp("--progress-bar", argv[arg]) || !strcmp("-b", argv[arg])) {
      cfg.prgbar = true;
    } else if (!strcmp("--progress-update-depth", argv[arg]) || !strcmp("-d", argv[arg])) {
      if (++arg == argc) {
        if (cfg.color)
          fputs("\n\x1b[31mError:\x1b[91m No value for progress-update-depth specified!\x1b[0m\n\n", stderr);
        else
          fputs("\nError: No value for progress-update-depth specified!\n\n", stderr);
        exit(EXIT_FAILURE);
      }
      cfg.prgdep = atoi(argv[arg]);
      if (cfg.prgdep < 1 || cfg.prgdep > SQUARES_NUM) {
        if (cfg.color)
          fputs("\n\x1b[31mError:\x1b[91m Progress-update-depth must be a numeric value and in range\x1b[0m 1 \x1b[91m-\x1b[0m 81\x1b[91m!\x1b[0m\n\n", stderr);
        else
          fputs("\nError: Progress-update-depth must be a numeric value and in range 1 - 81!\n\n", stderr);
        exit(EXIT_FAILURE);
      }
    } else if (!strcmp("--quiet", argv[arg]) || !strcmp("-q", argv[arg])) {
      cfg.quiet = true;
    } else if (!strcmp("--verbose", argv[arg]) || !strcmp("-v", argv[arg])) {
      cfg.verbose = true;
    } else if (!strcmp("--color", argv[arg]) || !strcmp("-c", argv[arg])) {
      cfg.color = true;
    } else if (!strcmp("--help", argv[arg]) || !strcmp("-h", argv[arg])) {
      if (cfg.color)
        printf("\n\x1b[31mUsage:\x1b[91m %s [\x1b[0moption...\x1b[91m] <\x1b[0mfile\x1b[91m>\x1b[0m\n  Solves the sudoku descriped by the input file\n\n\x1b[31mOptions:\x1b[0m\n  -\x1b[91mm\x1b[0m, --\x1b[91mmax-solutions <\x1b[0mnumber\x1b[91m>\x1b[0m          Stop after n solutions have been found\n  -\x1b[91mo\x1b[0m, --\x1b[91moutput <\x1b[0mfile\x1b[91m>\x1b[0m                   Print solutions to file\n\n  -\x1b[91mb\x1b[0m, --\x1b[91mprogress-bar\x1b[0m                    Show progress bar\n  -\x1b[91md\x1b[0m, --\x1b[91mprogress-update-depth <\x1b[0mnumber\x1b[91m>\x1b[0m  Precision of progress bar\n\n  -\x1b[91mq\x1b[0m, --\x1b[91mquiet\x1b[0m                           Only print errors\n  -\x1b[91mv\x1b[0m, --\x1b[91mverbose\x1b[0m                         Print out all found solutions\n  -\x1b[91mc\x1b[0m, --\x1b[91mcolor\x1b[0m                           Use ANSI escape sequences to display colors\n\n  -\x1b[91mh\x1b[0m, --\x1b[91mhelp\x1b[0m                            Print this page and quit\n\n", argv[0]);
      else
        printf("\nUsage: %s [option...] <file>\n  Solves the sudoku descriped by the input file\n\nOptions:\n  -m, --max-solutions <number>          Stop after n solutions have been found\n  -o, --output <file>                   Print solutions to file\n\n  -b, --progress-bar                    Show progress bar\n  -d, --progress-update-depth <number>  Precision of progress bar\n\n  -q, --quiet                           Only print errors\n  -v, --verbose                         Print out all found solutions\n  -c, --color                           Use ANSI escape sequences to display colors\n\n  -h, --help                            Print this page and quit\n\n", argv[0]);
      exit(EXIT_SUCCESS);
    } else if (*argv[arg] == '-') {
      if (cfg.color)
        fprintf(stderr, "\n\x1b[31mError:\x1b[91m Unknown option\x1b[0m %s\x1b[91m!\x1b[0m\n\n\x1b[32mHint:\x1b[92m Try\x1b[0m %s --help \x1b[92mfor help page.\x1b[0m\n\n", argv[arg], argv[0]);
      else
        fprintf(stderr, "\nError: Unknown option %s!\n\nHint: Try %s --help for help page.\n\n", argv[arg], argv[0]);
      exit(EXIT_FAILURE);
    } else {
      if (cfg.infile) {
        if (cfg.color)
          fputs("\n\x1b[31mError:\x1b[91m Multiple input files specified!\x1b[0m\n\n", stderr);
        else
          fputs("\nError: Multiple input files specified!\n\n", stderr);
        exit(EXIT_FAILURE);
      }
      cfg.infile = fopen(argv[arg], "r");
      if (!cfg.infile) {
        if (cfg.color)
          fprintf(stderr, "\n\x1b[31mError:\x1b[91m Couldn't open file\x1b[0m %s \x1b[91mfor reading!\x1b[0m\n\n", argv[arg]);
        else
          fprintf(stderr, "\nError: Couldn't open file %s for reading!\n\n", argv[arg]);
        exit(EXIT_FAILURE);
      }
    }

    ++arg;
  }

  if (!cfg.infile) {
    if (cfg.color)
      fputs("\n\x1b[31mError:\x1b[91m No input file specified!\x1b[0m\n\n", stderr);
    else
      fputs("\nError: No input file specified!\n\n", stderr);
    exit(EXIT_FAILURE);
  }

  if (cfg.prgbar && !cfg.color && !cfg.quiet)
      fputs("\nWarning: Cannot display progress bar without --color option!\n\n", stdout);

  if (cfg.prgdep > 9 && !cfg.quiet) {
    if (cfg.color)
      fputs("\n\x1b[33mWarning:\x1b[93m High progress-update-depth may result in graphical errors!\x1b[0m\n\n", stdout);
    else
      fputs("\nWarning: High progress-update-depth may result in graphical errors!\n\n", stdout);
  }

  return cfg;
}

int main(int argc, char **argv) {
  struct config cfg = prsargs(argc, argv);
  int s, i, end = rdfile(cfg.infile), buf[cfg.maxsol * SQUARES_NUM];
  pthread_t barthrd;

  if (!cfg.quiet) {
    if (cfg.color)
      fputs("\x1b[?25l\n\x1b[91m          < SUDOKU SOLVER >\x1b[31m\n\n  Read-in field:\x1b[0m\n", stdout);
    else
      fputs("\n          < SUDOKU SOLVER >\n\n  Read-in field:\n", stdout);
    wrtobuf(buf);
    prtbuf(buf, stdout);
    fputs("\n\n", stdout);

    if (cfg.prgbar && cfg.color) {
      done = false;
      pthread_create(&barthrd, NULL, &barrtn, &cfg);
    }
  }

  initgps();
  s = initvals(end) ? solve(end, cfg.maxsol, buf) : 0;

  if (!cfg.quiet) {
    if (cfg.prgbar && cfg.color) {
      done = true;
      pthread_join(barthrd, NULL);
      prtprg(pow(GROUP_SIZE, cfg.prgdep), cfg.prgdep);
    }

    if (cfg.color)
      printf("\x1b[0m─────────────────────────────────────\n\n\x1b[91mFound \x1b[0m%d\x1b[91m solution(s)...\x1b[0m\n", s);
    else
      printf("─────────────────────────────────────\n\nFound %d solution(s)...\n", s);
  }

  if (!cfg.quiet && cfg.verbose) {
    for (i = 0; i != s; ++i) {
      if (cfg.color)
        printf("\n  \x1b[31mSolution[\x1b[0m%d\x1b[31m]:\x1b[0m\n", i);
      else
        printf("\n  Solution[%d]:\n", i);
      prtbuf(buf + i * SQUARES_NUM, stdout);
    }
  }

  if (cfg.outfile) {
    for (i = 0; i != s; ++i)
      prtbuf(buf + i * SQUARES_NUM, cfg.outfile);

    fclose(cfg.outfile);
  }

  if (!cfg.quiet) {
    if (cfg.color)
      fputs("\n\x1b[91mDone.\x1b[0m\x1b[?25h\n\n", stdout);
    else
      fputs("\nDone.\n\n", stdout);
  }

  return EXIT_SUCCESS;
}

