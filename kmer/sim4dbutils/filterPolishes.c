#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "libbritypes.h"
#include "sim4polish.h"

#define MAX_SCAFFOLD   10000

char const *usage =
"usage: %s [-c c] [-i i] [-o o]\n"
"  -c c       Discard polishes below c%% composite.\n"
"  -i i       Discard polishes below i%% identity.\n"
"  -l l       Discard polishes below l identities.\n"
"\n"
"  -e e       Discard polishes below e exons.\n"
"  -E e       Discard polishes above e exons.\n"
"\n"
"  -C c       Discard polishes that are not from cDNA idx = c\n"
"  -G g       Discard polishes that are not from genomic idx = g\n"
"\n"
"  -o o       Write saved polishes to the 'o' file (default == stdout).\n"
"  -O         Don't write saved polishes.\n"
"  -d o       Write discarded polishes to the 'o' file (default == stdout).\n"
"  -q (or -D) Don't write discarded polishes.\n"
"  -j o       Write junk polishes to the 'o' file (junk == intractable and aborted).\n"
"\n"
"  -v         Report progress\n"
"\n"
"  -s         Segregate polishes by genomic idx.  Must be used with -o, will\n"
"             create numerous files 'o.%05d'.\n"
"\n"
"         Discarded polishes are printed to stdout (unless -q is supplied).\n"
"         All conditions must be met.\n"
"\n"
"   HINT: To filter by cDNA idx, use \"-c 0 -i 0 -l 0 -C idx\"\n"
"\n";


#ifdef TRUE64BIT
char const *msg1 = " Filter: %6.2f%% (%9lu matches processed) (%lu failed/intractable)\n";
char const *msg2 = " Filter: %6.2f%% (%9lu matches processed)\n";
char const *msg3 = "Filtering at %d%% coverage and %d%% identity and %dbp.\n";
char const *msg4 = "Filtering for cDNA == %d, genomic == %d\n";
char const *msg5 = "Genomic index %d larger than MAX_SCAFFOLD = %d!\n";
#else
char const *msg1 = " Filter: %6.2f%% (%9llu matches processed) (%llu failed/intractable)\r";
char const *msg2 = " Filter: %6.2f%% (%9llu matches processed)\r";
char const *msg3 = "Filtering at %ld%% coverage and %ld%% identity and %ldbp.\n";
char const *msg4 = "Filtering for cDNA == %ld, genomic == %ld\n";
char const *msg5 = "Genomic index %ld larger than MAX_SCAFFOLD = %d!\n";
#endif


int
main(int argc, char ** argv) {
  u32bit       arg  = 1;
  u32bit       minC = 0;
  u32bit       minI = 0;
  u32bit       minL = 0;
  s32bit       cdna = -1;
  s32bit       geno = -1;
  u32bit       minExons = 0;
  u32bit       maxExons = ~u32bitZERO;
  u32bit       beVerbose = 0;
  int          GOODsilent = 0;
  FILE        *GOOD       = stdout;
  int          CRAPsilent = 0;
  FILE        *CRAP       = stdout;
  FILE        *JUNK = 0L;
  sim4polish  *p;
  u64bit       pmod = 1;
  u64bit       good = 0;
  u64bit       crap = 0;
  u64bit       junk = 0;
  int          doSegregation = 0;
  char        *filePrefix = 0L;
  FILE       **SEGREGATE = 0L;

  arg = 1;
  while (arg < argc) {
    if        (strncmp(argv[arg], "-c", 2) == 0) {
      minC = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-i", 2) == 0) {
      minI = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-l", 2) == 0) {
      minL = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-e", 2) == 0) {
      minExons = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-E", 2) == 0) {
      maxExons = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-o", 2) == 0) {
      arg++;
      errno = 0;
      filePrefix = argv[arg];
      GOOD = fopen(argv[arg], "w");
      if (errno) {
        fprintf(stderr, "error: I couldn't open '%s' for saving good polishes.\n%s\n", argv[arg], strerror(errno));
        exit(1);
      }
      GOODsilent = 0;
    } else if (strncmp(argv[arg], "-O", 2) == 0) {
      GOODsilent = 1;
    } else if (strncmp(argv[arg], "-d", 2) == 0) {
      arg++;
      errno = 0;
      CRAP = fopen(argv[arg], "w");
      if (errno) {
        fprintf(stderr, "error: I couldn't open '%s' for saving discarded polishes.\n%s\n", argv[arg], strerror(errno));
        exit(1);
      }
      CRAPsilent = 0;
    } else if (strncmp(argv[arg], "-q", 2) == 0) {
      CRAPsilent = 1;
    } else if (strncmp(argv[arg], "-D", 2) == 0) {
      CRAPsilent = 1;
    } else if (strncmp(argv[arg], "-j", 2) == 0) {
      arg++;
      errno = 0;
      JUNK = fopen(argv[arg], "w");
      if (errno) {
        fprintf(stderr, "error: I couldn't open '%s' for saving junk polishes.\n%s\n", argv[arg], strerror(errno));
        exit(1);
      }
    } else if (strncmp(argv[arg], "-C", 2) == 0) {
      cdna = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-G", 2) == 0) {
      geno = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-verbose", 2) == 0) {
      beVerbose = 1;
    } else if (strncmp(argv[arg], "-segregate", 2) == 0) {
      doSegregation = 1;
      SEGREGATE = (FILE **)calloc(MAX_SCAFFOLD, sizeof(FILE *));
    }

    arg++;
  }

  if (isatty(fileno(stdin))) {
    fprintf(stderr, usage, argv[0]);
    exit(1);
  }

  if (!CRAPsilent && !GOODsilent && (fileno(GOOD) == fileno(CRAP))) {
    fprintf(stderr, "error: filter has no effect; saved and discarded polishes\n");
    fprintf(stderr, "       both printed to the same place!\n");
    fprintf(stderr, "       (try using one of -o, -O, -d, -D)\n");
    exit(1);
  }

  if (doSegregation && (filePrefix == 0L)) {
    fprintf(stderr, "error: you must specify a file prefix when segregating (-s requires -o)\n");
    exit(1);
  }


  if (beVerbose) {
    fprintf(stderr, msg3, minC, minI, minL);
    fprintf(stderr, msg4, cdna, geno);
  }

  while ((p = s4p_readPolish(stdin)) != 0L) {

    if (JUNK && ((p->strandOrientation == SIM4_STRAND_INTRACTABLE) ||
                 (p->strandOrientation == SIM4_STRAND_FAILED))) {
      junk++;
      s4p_printPolish(JUNK, p, S4P_PRINTPOLISH_FULL);
    } else {
      if ((p->percentIdentity  >= minI) &&
          (p->querySeqIdentity >= minC) &&
          (p->numMatches  >= minL) &&
          ((cdna == -1) || (cdna == p->estID)) &&
          ((geno == -1) || (geno == p->genID)) &&
          (minExons <= p->numExons) &&
          (p->numExons <= maxExons)) {
        good++;
        if (doSegregation) {
          if (p->genID >= MAX_SCAFFOLD) {
            fprintf(stderr, msg5, p->genID, MAX_SCAFFOLD);
          } else {
            if (SEGREGATE[p->genID] == 0L) {
              char filename[1024];
              sprintf(filename, "%s.%04d", filePrefix, p->genID);
              errno = 0;
              SEGREGATE[p->genID] = fopen(filename, "w");
              if (errno) {
                fprintf(stderr, "Error: Couldn't open '%s'\n%s\n", filename, strerror(errno));
                exit(1);
              }
            }
            s4p_printPolish(SEGREGATE[p->genID], p, S4P_PRINTPOLISH_FULL);
          }
        } else {
          if (!GOODsilent)
            s4p_printPolish(GOOD, p, S4P_PRINTPOLISH_FULL);
        }
      } else {
        crap++;
        if (!CRAPsilent)
          s4p_printPolish(CRAP, p, S4P_PRINTPOLISH_FULL);
      }
    }

    if ((beVerbose) && ((good+crap) == pmod)) {
      pmod += 8888 + (random() % 1000);
      if (junk > 0)
        fprintf(stderr, msg1, 100.0 * good / (good+crap), good+crap, junk);
      else
        fprintf(stderr, msg2, 100.0 * good / (good+crap), good+crap);
      fflush(stderr);
    }

    s4p_destroyPolish(p);
  }

  if (beVerbose) {
    if (junk > 0)
      fprintf(stderr, msg1, 100.0 * good / (good+crap), good+crap, junk);
    else
      fprintf(stderr, msg2, 100.0 * good / (good+crap), good+crap);
    fprintf(stderr, "\n");
  }

  if (GOOD)
    fclose(GOOD);
  if (JUNK)
    fclose(JUNK);

  return(0);
}
