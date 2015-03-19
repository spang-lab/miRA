

#include <stdlib.h>

#ifndef UTIL_H
#define UTIL_H

enum log_level {
  LOG_LEVEL_QUIET = 0,
  LOG_LEVEL_BASIC = 1,
  LOG_LEVEL_VERBOSE = 2,
};

struct configuration_params {
  int log_level;
  int openmp_thread_count;
  int cluster_gap_size;
  int cluster_min_reads;
  int cluster_window_size;
  int cluster_max_length;

  int max_precursor_length;
  int min_precursor_length;
  double min_mfe_per_nt;
  int max_hairpin_count;
  int min_double_strand_length;
  int permutation_count;
  double max_pvalue;
  double min_coverage;
  double min_paired_fraction;
  int min_mature_strand_length;
  int max_mature_strand_length;
  int allow_three_mismatches;
  int allow_two_terminal_mismatches;

  int create_coverage_plots;
  int create_structure_images;
  int create_coverage_images;
  int cleanup_auxiliary_files;
};

int initialize_configuration(struct configuration_params **config,
                             char *config_file);

int reverse_complement_sequence_string(char **result, char *seq, size_t n);
int create_file_path(char **file_path, const char *path, const char *filename);

void log_configuration(struct configuration_params *config);
void log_basic(int loglevel, const char *msg, ...);
void log_verbose(int loglevel, const char *msg, ...);

double mean(double *list, int n);
double sd(double *list, int n, double mean);
double pvalue(double mean, double sd, double value);

#endif