#include "reporting.h"
#include "coverage.h"
#include "candidates.h"
#include "errors.h"
#include "defs.h"
#include <stdio.h>
#include "../config.h"
#include "math.h"
#include "util.h"
#include <sys/stat.h>
#include <errno.h>

int report_valid_candiates(struct extended_candidate_list *ec_list,
                           struct chrom_coverage **coverage_table,
                           const char *executable_path, const char *output_path,
                           struct configuration_params *config) {

  char *cov_plot_path = NULL;
  char *structure_path = NULL;
  char *coverage_path = NULL;
  char *report_path = NULL;

  int err = create_output_directory_structure(&cov_plot_path, &structure_path,
                                              &coverage_path, &report_path,
                                              output_path);
  if (err) {
    return err;
  }
  char bed_result_filename[] = "final_candidates.bed";
  char *bed_file = NULL;
  create_file_path(&bed_file, output_path, bed_result_filename);
  FILE *bed_fp = fopen(bed_file, "w");
  free(bed_file);
  char html_result_filename[] = "final_candidates.html";
  char *html_file = NULL;
  create_file_path(&html_file, output_path, html_result_filename);
  FILE *html_fp = fopen(html_file, "w");
  free(html_file);
  char gtf_result_filename[] = "final_candidates.gtf";
  char *gtf_file = NULL;
  create_file_path(&gtf_file, output_path, gtf_result_filename);
  FILE *gtf_fp = fopen(gtf_file, "w");
  free(gtf_file);

  inititalize_html_report(html_fp);

  struct extended_candidate *ecand = NULL;
  struct chrom_coverage *chrom_cov = NULL;
  for (size_t i = 0; i < ec_list->n; i++) {

    // temporary
    ecand = ec_list->candidates[i];
    if (ecand->possible_micro_rnas->n == 0) {
      continue;
    }
    ecand->mature_micro_rna = ecand->possible_micro_rnas->mature_sequences[0];
    ecand->star_micro_rna = ecand->mature_micro_rna->matching_sequence;
    if (ecand->is_valid != 1) {
      continue;
    }
    HASH_FIND_STR(*coverage_table, ecand->cand->chrom, chrom_cov);
    if (chrom_cov == NULL) {
      free(cov_plot_path);
      free(structure_path);
      free(coverage_path);
      free(report_path);
      return E_CHROMOSOME_NOT_FOUND;
    }
    create_candidate_report(ecand, chrom_cov, executable_path, cov_plot_path,
                            structure_path, coverage_path, report_path, config);
    write_bed_lines(bed_fp, ecand);
    write_gtf_line(gtf_fp, ecand);
    write_html_table_row(html_fp, ecand);
  }
  if (bed_fp != NULL) {
    fclose(bed_fp);
  }
  if (gtf_fp != NULL) {
    fclose(gtf_fp);
  }
  finalize_html_report(html_fp);
  if (html_fp != NULL) {
    fclose(html_fp);
  }
  free(cov_plot_path);
  free(structure_path);
  free(coverage_path);
  free(report_path);

  return E_SUCCESS;
}

int create_output_directory_structure(char **cov_plot_ouput_path,
                                      char **structure_output_path,
                                      char **coverage_output_path,
                                      char **report_output_path,
                                      const char *output_path) {
  int err = 0;
  err = create_directory_if_ne(output_path);
  if (err) {
    return err;
  }
  const char COV_PLOT_SUBDIR[] = "coverage_plots/";
  create_file_path(cov_plot_ouput_path, output_path, COV_PLOT_SUBDIR);
  err = create_directory_if_ne(*cov_plot_ouput_path);
  if (err) {
    return err;
  }
  const char STRUCTURE_SUBDIR[] = "structure_plots/";
  create_file_path(structure_output_path, output_path, STRUCTURE_SUBDIR);
  err = create_directory_if_ne(*structure_output_path);
  if (err) {
    return err;
  }
  const char COVERAGE_SUBDIR[] = "coverage_structures/";
  create_file_path(coverage_output_path, output_path, COVERAGE_SUBDIR);
  err = create_directory_if_ne(*coverage_output_path);
  if (err) {
    return err;
  }
  const char REPORT_SUBDIR[] = "reports/";
  create_file_path(report_output_path, output_path, REPORT_SUBDIR);
  err = create_directory_if_ne(*report_output_path);
  if (err) {
    return err;
  }

  return E_SUCCESS;
}

int create_directory_if_ne(const char *path) {
  int err = 0;

  err = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  /**
   * if mkdir failed and if the directory does not already exist, something went
   * wrong */
  if (err == -1 && errno != EEXIST) {
    return E_CREATING_DIRECTORY_FAILED;
  }
  return E_SUCCESS;
}

int create_candidate_report(
    struct extended_candidate *ecand, struct chrom_coverage *chrom_cov,
    const char *executable_path, const char *cov_plot_ouput_path,
    const char *structure_output_path, const char *coverage_output_path,
    const char *report_output_path, struct configuration_params *config) {
  char *cov_plot_file = NULL;
  char *structure_file = NULL;
  char *coverage_file = NULL;
  char *tex_file = NULL;
  int err;

  log_basic_timestamp(config->log_level,
                      "\tGenerating report for cluster %lld...\n",
                      ecand->cand->id);
#ifdef HAVE_GNUPLOT
  if (config->create_coverage_plots) {
    log_verbose_timestamp(config->log_level,
                          "\t\tGenerating coverage plot...\n");
    err = create_coverage_plot(&cov_plot_file, ecand, chrom_cov,
                               cov_plot_ouput_path);
    if (err != E_SUCCESS) {
      cov_plot_file = NULL;
    }
  }
#endif /* HAVE_GNUPLOT */
#ifdef HAVE_JAVA
  if (config->create_structure_plots) {
    log_verbose_timestamp(config->log_level,
                          "\t\tGenerating structure image...\n");
    err = create_structure_image(&structure_file, ecand, executable_path,
                                 structure_output_path);
    if (err != E_SUCCESS) {
      structure_file = NULL;
    }
  }
#endif /* HAVE_JAVA */

#ifdef HAVE_JAVA
  if (config->create_structure_coverage_plots) {
    log_verbose_timestamp(config->log_level,
                          "\t\tGenerating coverage image...\n");
    err = create_coverage_image(&coverage_file, ecand, executable_path,
                                chrom_cov, coverage_output_path);
    if (err != E_SUCCESS) {
      coverage_file = NULL;
    }
  }
#endif /* HAVE_JAVA */
  log_verbose_timestamp(config->log_level,
                        "\t\tGenerating latex_template...\n");
  err =
      create_latex_template(&tex_file, ecand, chrom_cov, cov_plot_file,
                            structure_file, coverage_file, report_output_path);
  if (err != E_SUCCESS) {
    cleanup_auxiliary_files(cov_plot_file, structure_file, coverage_file,
                            tex_file, config);
    return E_CREATING_REPORT_FAILED;
  }

#ifdef HAVE_LATEX
  log_verbose_timestamp(config->log_level, "\t\tCompiling report...\n");
  err = compile_tex_file(tex_file, report_output_path);
  if (err != E_SUCCESS) {
    cleanup_auxiliary_files(cov_plot_file, structure_file, coverage_file,
                            tex_file, config);
    return E_CREATING_REPORT_FAILED;
  }
#endif /* HAVE_LATEX */
  cleanup_auxiliary_files(cov_plot_file, structure_file, coverage_file,
                          tex_file, config);

  return E_SUCCESS;
}
int create_coverage_plot(char **result_file, struct extended_candidate *ecand,
                         struct chrom_coverage *chrom_cov,
                         const char *output_path) {
  struct micro_rna_candidate *cand = ecand->cand;
  struct candidate_subsequence *mature_mirna = ecand->mature_micro_rna;
  struct candidate_subsequence *star_mirna = ecand->star_micro_rna;

  char data_file_name[265];
  sprintf(data_file_name, "Cluster_%lld_coverage_data.dat", cand->id);
  char *data_file_path = NULL;
  create_file_path(&data_file_path, output_path, data_file_name);

  char file_name[265];
  sprintf(file_name, "Cluster_%lld_coverage_plot.png", cand->id);
  char *file_path = NULL;
  create_file_path(&file_path, output_path, file_name);

  size_t l = cand->end - cand->start;
  u64 x_start = cand->start;
  u64 x_end = cand->end;
  u32 x2_start = 0;
  u32 x2_end = (u32)l;

  u64 box1_start = mature_mirna->start + cand->start + 1;
  u64 box1_end = mature_mirna->end + cand->start;

  u64 box2_start = star_mirna->start + cand->start + 1;
  u64 box2_end = star_mirna->end + cand->start;

  u32 *plus_cov = chrom_cov->coverage_plus + cand->start;
  u32 *minus_cov = chrom_cov->coverage_minus + cand->start;

  FILE *fp = fopen(data_file_path, "w");
  if (fp == NULL) {
    return E_FILE_WRITING_FAILED;
  }
  for (size_t i = 0; i < l; i++) {
    size_t global_index = cand->start + i;
    fprintf(fp, "%ld\t%d\t%d\t%ld\t%c\n", global_index + 1, plus_cov[i],
            minus_cov[i], i + 1, cand->sequence[i]);
  }
  fclose(fp);

  u32 y_max = 0;
  for (size_t i = 0; i < l; i++) {
    if (plus_cov[i] > y_max) {
      y_max = plus_cov[i];
    }
    if (minus_cov[i] > y_max) {
      y_max = minus_cov[i];
    }
  }
  y_max = (u32)(y_max * 1.5 * 1.5);

  char gnuplot_system_call[4096];
  sprintf(gnuplot_system_call,
          "gnuplot -p -e \""
          "set terminal png font 'Verdana,10';\n"
          "set output '%s';\n"
          " set xtics nomirror;\n"
          " set x2tics;\n"
          " set format x '%%s';\n"
          " set autoscale xfix;\n"
          " set autoscale x2fix;\n"
          " set style rect fc rgb '#084081' fs transparent solid 0.1 "
          "noborder;\n"
          " set logscale y;\n"
          " set obj rect from %lld, graph 0 to %lld, graph 1;\n"
          " set obj rect from %lld, graph 0 to %lld, graph 1;\n"
          " set yrange [0.1:%d];\n"
          " set xrange [%lld:%lld];\n"
          " set x2range [%d:%d];\n"
          " set xlabel 'Genome position (1-based)';\n"
          " set x2label 'Sequence position (1-based)';\n"
          " set ylabel 'Coverage (per nucleotide)';\n"
          " set key left top;\n"
          " plot '%s' using 1:2 with histeps lw 2 linecolor rgb 'red' title "
          "'plus','' using 4:2 with histeps lw 2 linecolor rgb 'red' axes x2y1 "
          "notitle, '' using 1:3 with histeps lw 2 linecolor rgb 'blue' title "
          "'minus', '' using 4:3 with histeps lw 2 linecolor rgb 'blue' "
          "notitle\" ",
          file_path, box1_start, box1_end, box2_start, box2_end, y_max, x_start,
          x_end, x2_start, x2_end, data_file_path);
  int err = system(gnuplot_system_call);
  free(data_file_path);
  if (err != E_SUCCESS) {
    free(file_path);
    return E_GNUPLOT_SYSTEM_CALL_FAILED;
  }
  *result_file = file_path;
  return E_SUCCESS;
}

int create_structure_image(char **result_file, struct extended_candidate *ecand,
                           const char *executable_path,
                           const char *output_path) {
  const int SYS_CALL_MAX_LENGHT = 4096;
  char *varna_path = NULL;
  create_file_path(&varna_path, executable_path, "VARNAv3-91.jar");
  const char varna_class_name[] = "fr.orsay.lri.varna.applications.VARNAcmd";
  const char varna_algorith[] = "naview";
  const char varna_auto_interior_loops[] = "True";
  const char varna_auto_terminal_loops[] = "True";
  const int varna_resolution = 10;
  const char hex_color_red[] = "#FF0000";
  const char hex_color_blue[] = "#0000FF";

  struct micro_rna_candidate *cand = ecand->cand;
  struct candidate_subsequence *mature_mirna = ecand->mature_micro_rna;
  struct candidate_subsequence *star_mirna = ecand->star_micro_rna;

  char file_name[265];
  sprintf(file_name, "Cluster_%lld_structure_image.eps", cand->id);
  char *file_path = NULL;
  create_file_path(&file_path, output_path, file_name);

  u32 m_start = mature_mirna->start + 1;
  u32 m_end = mature_mirna->end;
  u32 s_start = star_mirna->start + 1;
  u32 s_end = star_mirna->end;
  char *seq = cand->sequence;
  char *structure = cand->structure;

  char java_cmd[20] = "java";
#ifdef JAVA_VM_COMMAND
  sprintf(java_cmd, "%s", JAVA_VM_COMMAND);
#endif

  char java_system_call[4096];
  snprintf(java_system_call, SYS_CALL_MAX_LENGHT,
           "java -Dapple.awt.UIElement=\"true\" -cp %s %s -algorithm %s "
           "-autoInteriorLoops %s -error False -warning False "
           "-autoTerminalLoops %s -resolution %d -highlightRegion "
           "\"%d-%d:fill=%s;%d-%d:fill=%s\" -title \"Cluster %lld\" "
           "-sequenceDBN \"%s\" "
           "-structureDBN \"%s\" -o %s ",
           varna_path, varna_class_name, varna_algorith,
           varna_auto_interior_loops, varna_auto_terminal_loops,
           varna_resolution, m_start, m_end, hex_color_red, s_start, s_end,
           hex_color_blue, cand->id, seq, structure, file_path);
  free(varna_path);
  int err = system(java_system_call);

  if (err != 0) {
    free(file_path);
    return E_JAVA_SYSTEM_CALL_FAILED;
  }
  *result_file = file_path;
  return E_SUCCESS;
}

int create_coverage_image(char **result_file, struct extended_candidate *ecand,
                          const char *executable_path,
                          struct chrom_coverage *chrom_cov,
                          const char *output_path) {
  char *varna_path = NULL;
  create_file_path(&varna_path, executable_path, "VARNAv3-91.jar");
  const char varna_class_name[] = "fr.orsay.lri.varna.applications.VARNAcmd";
  const char varna_algorith[] = "naview";
  const char varna_auto_interior_loops[] = "True";
  const char varna_auto_terminal_loops[] = "True";
  const int varna_resolution = 10;
  const char *coverage_colors[] = {"#EVEVFF", "#C2C2FF", "#ADADFF",
                                   "#7070FF", "#3333FF", "#0000F5",
                                   "#0000B8", "#00007A", "#00003D"};
  const char hex_color_white[] = "#FFFFFF";
  struct micro_rna_candidate *cand = ecand->cand;

  char *seq = cand->sequence;
  char *structure = cand->structure;

  u32 *cov_list = chrom_cov->coverage_plus;
  if (cand->strand == '-') {
    cov_list = chrom_cov->coverage_minus;
  }

  size_t segment_n = 50;
  size_t cand_n = cand->end - cand->start;
  u32 color_index = 0;

  char *highlight_string = (char *)malloc(segment_n * cand_n * sizeof(char));
  char *write_point = highlight_string;
  for (size_t i = 0; i < cand_n; i++) {
    size_t global_index = cand->start + i;
    map_coverage_to_color_index(&color_index, cov_list[global_index]);
    write_point +=
        sprintf(write_point, "%ld-%ld:fill=%s,outline=%s;", i + 1, i + 1,
                coverage_colors[color_index], hex_color_white);
  }
  char file_name[265];
  sprintf(file_name, "Cluster_%lld_coverage_image.eps", cand->id);
  char *file_path = NULL;
  create_file_path(&file_path, output_path, file_name);

  char java_cmd[20] = "java";
#ifdef JAVA_VM_COMMAND
  sprintf(java_cmd, "%s", JAVA_VM_COMMAND);
#endif
  size_t sys_call_n = 4096 + segment_n * cand_n;
  char *java_system_call = (char *)malloc(sys_call_n * sizeof(char));
  snprintf(java_system_call, sys_call_n,
           "java -Dapple.awt.UIElement=\"true\" -cp %s %s -algorithm %s "
           "-autoInteriorLoops %s -error False -warning False "
           "-autoTerminalLoops %s -resolution %d -highlightRegion "
           "\"%s\" -title \"Cluster %lld\" "
           "-sequenceDBN \"%s\" "
           "-structureDBN \"%s\" -o %s ",
           varna_path, varna_class_name, varna_algorith,
           varna_auto_interior_loops, varna_auto_terminal_loops,
           varna_resolution, highlight_string, cand->id, seq, structure,
           file_path);
  free(varna_path);
  int err = system(java_system_call);
  free(highlight_string);
  free(java_system_call);
  if (err != 0) {
    free(file_path);
    return E_JAVA_SYSTEM_CALL_FAILED;
  }
  *result_file = file_path;
  return E_SUCCESS;
}

int create_latex_template(char **tex_file, struct extended_candidate *ecand,
                          struct chrom_coverage *chrom_cov,
                          const char *cov_plot_file, const char *structure_file,
                          const char *coverage_file, const char *output_path) {

  struct micro_rna_candidate *cand = ecand->cand;
  struct candidate_subsequence *mature_mirna = ecand->mature_micro_rna;
  struct candidate_subsequence *star_mirna = ecand->star_micro_rna;
  struct unique_read_list *mature_reads = ecand->mature_micro_rna->reads;
  struct unique_read_list *star_reads = ecand->star_micro_rna->reads;

  char file_name[265];
  sprintf(file_name, "Cluster_%lld_report.tex", cand->id);
  char *file_path = NULL;
  create_file_path(&file_path, output_path, file_name);
  FILE *fp = fopen(file_path, "w");
  if (fp == NULL) {
    free(file_path);
    return E_FILE_WRITING_FAILED;
  }
  fprintf(fp, "\\documentclass[a4paper]{article}\n"
              "\\usepackage[margin=2cm,nohead,nofoot]{geometry}\n"
              "\\usepackage{graphicx}\n"
              "\\usepackage{color}\n"
              "\\parindent 0pt\n"
              "\\begin{document}\n"
              "\\section*{Summary}\n"
              "\\verb$Cluster %lld$\\\\\n"
              "\\verb$%s$\\\\\n"
              "Precursor miRNA : start = \\verb$%lld$, stop = \\verb$%lld$, "
              "length = \\verb$%lld$\\\\\n"
              "Best mature miRNA : start = \\verb$%lld$ (\\verb$%d$), stop = "
              "\\verb$%lld$ (\\verb$%d$), length = \\verb$%d$, arm = "
              "\\verb$%d'$\\\\\n"
              "Best star miRNA : start = \\verb$%lld$ (\\verb$%d$), stop = "
              "\\verb$%lld$ (\\verb$%d$), length = \\verb$%d$\\\\\n"
              "{[}Note: Positions are 1-based and inclusive{]}\\\\\n \\\\\n",
          cand->id, cand->chrom, cand->start, cand->end,
          cand->start - cand->end, cand->start + mature_mirna->start,
          mature_mirna->start, cand->start + mature_mirna->end,
          mature_mirna->end, mature_mirna->end - mature_mirna->start,
          (int)mature_mirna->arm, cand->start + star_mirna->start,
          star_mirna->start, cand->start + star_mirna->end, star_mirna->end,
          star_mirna->end - star_mirna->start);
  fprintf(fp, "Best mature miRNA sequence: \\verb$");
  for (u32 i = mature_mirna->start; i < mature_mirna->end; i++) {
    fprintf(fp, "%c", cand->sequence[i]);
  }
  fprintf(fp, "$\\\\\n");
  fprintf(fp, "Best star miRNA sequence: \\verb$");
  for (u32 i = star_mirna->start; i < star_mirna->end; i++) {
    fprintf(fp, "%c", cand->sequence[i]);
  }
  fprintf(fp, "$\\\\\n");

  fprintf(fp, "\\\\\n"
              "MFE = \\verb$%9.7e$ kcal/mol/nt\\\\\n"
              "p-value = \\verb$%9.7e$\\\\\n"
              "Null distribution: Mean = \\verb$%9.7e$ kcal/mol/nt, "
              "StandDev = \\verb$%9.7e$ kcal/mol/nt\\\\\n"
              "\\\\\n"
              "Number of terminal loops = \\verb$%i$\\\\\n"
              "Longest ds segment (0 MM): start = \\verb$%i$, stop = "
              "\\verb$%i$, length = \\verb$%i$\\\\\n"
              "Longest ds segment (2 MM): start = \\verb$%i$, stop = "
              "\\verb$%i$, length = \\verb$%i$\\\\\n"
              "Total fraction of paired nucleotides = %5.4f\\\\\n",
          cand->mfe, cand->pvalue, cand->mean, cand->sd,
          cand->external_loop_count, cand->stem_start, cand->stem_end,
          cand->stem_end - cand->stem_start + 1, cand->stem_start_with_mismatch,
          cand->stem_end_with_mismatch,
          cand->stem_end_with_mismatch - cand->stem_start_with_mismatch + 1,
          cand->paired_fraction);

  fprintf(fp, "All micro rna candidates: \\\\ \\vspace{0.5cm}\n "
              "\\begin{tabular}{| c | c | c | c | c "
              "|}\\hline \nSequence & Type & "
              "Start & Stop & Passes Dicer check \\\\ \\hline\n");
  struct candidate_subsequence_list *css_list = ecand->possible_micro_rnas;
  struct candidate_subsequence *mature_tmp = NULL;
  struct candidate_subsequence *star_tmp = NULL;
  for (size_t i = 0; i < css_list->n; i++) {
    mature_tmp = css_list->mature_sequences[i];
    star_tmp = mature_tmp->matching_sequence;
    fprintf(fp, "\\verb$");
    for (u32 i = mature_tmp->start; i < mature_tmp->end; i++) {
      fprintf(fp, "%c", cand->sequence[i]);
    }
    fprintf(fp, "$ & \\verb$mature$ & \\verb$%lld$ & \\verb$%lld$ & \\verb$%s$ "
                "\\\\ \n",
            cand->start + mature_tmp->start, cand->start + mature_tmp->end, "");
    fprintf(fp, "\\verb$");
    for (u32 i = star_tmp->end - 1; i >= star_tmp->start; i--) {
      fprintf(fp, "%c", cand->sequence[i]);
    }
    fprintf(fp, "$ & \\verb$star$ & \\verb$%lld$ & \\verb$%lld$ & \\verb$%s$ "
                "\\\\ \\hline\n",
            cand->start + star_tmp->start, cand->start + star_tmp->end,
            star_tmp->is_artificial ? "false" : "true");
  }
  fprintf(fp, "\\end{tabular}\\\\\n");

  u32 *cov_list = chrom_cov->coverage_plus;
  if (cand->strand == '-') {
    cov_list = chrom_cov->coverage_minus;
  }

  u64 total_coverage = 0;
  get_coverage_in_range(&total_coverage, cov_list, cand->start, cand->end);

  fprintf(fp,
          "\\section*{Coverage}\n"
          "Mean coverage precursor miRNA: \\verb$%5.4f$ per nucleotide\\\\\n"
          "Mean coverage mature miRNA: \\verb$%5.4f$ per nucleotide\\\\\n"
          "Mean coverage star miRNA: \\verb$%5.4f$ per nucleotide\\\\\n"
          "Total number of reads for precursor miRNA:\\verb$ %d (%3.2e%% of "
          "total reads)$\\\\\\n",
          (double)total_coverage / (cand->end - cand->start),
          (double)mature_mirna->coverage /
              (mature_mirna->end - mature_mirna->start),
          (double)star_mirna->coverage / (star_mirna->end - star_mirna->start),
          ecand->total_reads, ecand->total_read_percent);
  if (cov_plot_file != NULL) {
    fprintf(fp, "\\begin{center}\n"
                "\\includegraphics[width=\\textwidth]{%s}\n"
                "\\end{center}\\newpage\n",
            cov_plot_file);
  } else {
    fprintf(fp, "{[}Note: Coverage Plot was not included because gnuplot could "
                "not be found or the gnuplot call failed{]}\\\\\n \\\\\n"
                "\\newpage\n");
  }

  fprintf(fp, "\\subsection*{Read alignment: Mature sequence}\n");
  write_unique_reads_to_tex(fp, cand, mature_mirna, mature_reads);
  fprintf(fp, "\\newpage\n");
  fprintf(fp, "\\subsection*{Read alignment: Star sequence}\n");
  fprintf(fp, "\\section*{Structure}\n");
  write_unique_reads_to_tex(fp, cand, star_mirna, star_reads);
  if (structure_file != NULL) {
    fprintf(fp, "\\begin{center}\n"
                "\\includegraphics[height=\\textheight,width=\\textwidth,"
                "keepaspectratio]{%s}\n"
                "\\end{center}\n",
            structure_file);
  } else {
    fprintf(fp, "{[}Note: Structure was not included because java could "
                "not be found or the Varna call failed{]}\\\\\n \\\\\n");
  }
  if (coverage_file != NULL) {
    fprintf(fp, "\\begin{center}\n"
                "\\includegraphics[height=\\textheight,width=\\textwidth,"
                "keepaspectratio]{%s}\n"
                "\\end{center}\n",
            coverage_file);
  } else {
    fprintf(fp,
            "{[}Note: Coverage Structure was not included because java could "
            "not be found or the Varna call failed{]}\\\\\n \\\\\n");
  }
  fprintf(fp, "\\end{document}");

  fclose(fp);
  *tex_file = file_path;
  return E_SUCCESS;
}

int write_unique_reads_to_tex(FILE *fp, struct micro_rna_candidate *cand,
                              struct candidate_subsequence *subseq,
                              struct unique_read_list *reads) {
  size_t total_length = cand->end - cand->start;
  const int PRINT_FLANK = 30;
  u32 start;
  if (subseq->start < PRINT_FLANK) {
    start = 0;
  } else {
    start = subseq->start - PRINT_FLANK;
  }
  u32 end = subseq->end + PRINT_FLANK;
  if (end > total_length) {
    end = total_length - 1;
  }
  fprintf(fp, "\\verb$");
  for (u32 i = start; i < end; i++) {
    if (i >= subseq->start && i < subseq->end) {
      fprintf(fp, "%c", cand->sequence[i]);
    } else {
      fprintf(fp, ".");
    }
  }
  fprintf(fp, "$\\\\\n \\verb$");
  for (u32 i = start; i < end; i++) {
    fprintf(fp, "%c", cand->sequence[i]);
  }
  fprintf(fp, "$\\\\\n");
  struct unique_read *read = NULL;
  for (size_t i = 0; i < reads->n; i++) {
    read = reads->reads[i];
    fprintf(fp, "\\verb$");
    for (u32 i = start; i < end; i++) {
      if (i >= read->start - cand->start && i < read->end - cand->start) {
        u32 local_index = i - (read->start - cand->start);
        fprintf(fp, "%c", read->seq[local_index]);
      } else {
        fprintf(fp, ".");
      }
    }
    fprintf(fp, "$[$\\times$ %i]\\\\\n", read->count);
  }

  return E_SUCCESS;
}

int compile_tex_file(const char *tex_file_path, const char *output_path) {
  char latex_system_call[2048];
  sprintf(
      latex_system_call,
      "pdflatex -interaction=batchmode -output-directory=\"%s\" %s >/dev/null",
      output_path, tex_file_path);
  int err = system(latex_system_call);
  if (err != 0) {
    return E_LATEX_SYSTEM_CALL_FAILED;
  }
  return E_SUCCESS;
}

int map_coverage_to_color_index(u32 *result, u32 coverage) {
  if (coverage < 1) {
    coverage = 1;
  }
  double log_cov = log10(coverage);
  int index = (int)floor(log_cov * 5.0 + 0.5);
  if (index < 0) {
    index = 0;
  }
  if (index > 8) {
    index = 8;
  }
  *result = index;
  return E_SUCCESS;
}

int write_bed_lines(FILE *fp, struct extended_candidate *ecand) {
  if (fp == NULL) {
    return E_FILE_WRITING_FAILED;
  }
  struct micro_rna_candidate *cand = ecand->cand;
  struct candidate_subsequence_list *css_list = ecand->possible_micro_rnas;
  struct candidate_subsequence *mature_mirna = NULL;
  struct candidate_subsequence *star_mirna = NULL;
  fprintf(fp, "%s\t%llu\t%llu\tprecursor_%lld\t%d\t%c\t%llu\t%llu\t%d\t%d\n",
          cand->chrom, cand->start, cand->end, cand->id, 0, cand->strand,
          cand->start, cand->end, 0, 0);
  for (size_t i = 0; i < css_list->n; i++) {
    mature_mirna = css_list->mature_sequences[i];
    star_mirna = mature_mirna->matching_sequence;
    u32 score = (star_mirna->is_artificial) ? 500 : 999;

    fprintf(fp,
            "%s\t%llu\t%llu\tprecursor_%lld_mir_%ld_%s\t%d\t%c\t%llu\t%llu\n",
            cand->chrom, cand->start, cand->end, cand->id, i,
            (mature_mirna->arm == 5) ? "5p" : "3p", score, cand->strand,
            cand->start + mature_mirna->start, cand->start + mature_mirna->end);
    fprintf(fp,
            "%s\t%llu\t%llu\tprecursor_%lld_mir_%ld_%s\t%d\t%c\t%llu\t%llu\n",
            cand->chrom, cand->start, cand->end, cand->id, i,
            (mature_mirna->arm == 5) ? "3p" : "5p", score, cand->strand,
            cand->start + star_mirna->start, cand->start + star_mirna->end);
  }

  return E_SUCCESS;
}
int write_gtf_line(FILE *fp, struct extended_candidate *ecand) {
  if (fp == NULL) {
    return E_FILE_WRITING_FAILED;
  }
  struct micro_rna_candidate *cand = ecand->cand;
  struct unique_read_list *mature_reads = ecand->mature_micro_rna->reads;
  struct unique_read_list *star_reads = ecand->star_micro_rna->reads;

  u32 mature_read_count = 0;
  for (size_t i = 0; i < mature_reads->n; i++) {
    mature_read_count += mature_reads->reads[i]->count;
  }
  u32 star_read_count = 0;
  for (size_t i = 0; i < star_reads->n; i++) {
    star_read_count += star_reads->reads[i]->count;
  }

  fprintf(
      fp,
      "%s\tmiRA\texon\t%lld\t%lld\t0\t%c\t.\tgene_id \"precursor_%lld_%s\"\n",
      cand->chrom, cand->start + 1, cand->end, cand->strand, cand->id,
      (cand->strand == '+') ? "plus" : "minus");

  return E_SUCCESS;
}

int write_json_entry(FILE *fp, struct extended_candidate *ecand) {
  if (fp == NULL) {
    return E_FILE_WRITING_FAILED;
  }
  struct micro_rna_candidate *cand = ecand->cand;
  struct candidate_subsequence *mature_mirna = ecand->mature_micro_rna;
  struct candidate_subsequence *star_mirna = ecand->star_micro_rna;

  fprintf(fp, "\"Cluster_%lld_%s\":{\n", cand->id,
          (cand->strand == '-') ? "minus" : "plus");
  fprintf(fp, "\t\"chromosome\":\"%s\",\n", cand->chrom);
  fprintf(fp, "\t\"candidate_start\":%lld,\n", cand->start);
  fprintf(fp, "\t\"candidate_end\":%lld,\n", cand->end);
  fprintf(fp, "\t\"candidate_length\":%lld,\n", cand->end - cand->start);
  fprintf(fp, "\t\"strand\":\"%c\",\n", cand->strand);
  fprintf(fp, "\t\"mature_sequence\":\"");
  for (u32 i = mature_mirna->start; i < mature_mirna->end; i++) {
    fprintf(fp, "%c", cand->sequence[i]);
  }
  fprintf(fp, "\",\n");
  fprintf(fp, "\t\"mature_start\":%lld,\n", cand->start + mature_mirna->start);
  fprintf(fp, "\t\"mature_end\":%lld,\n", cand->start + mature_mirna->end);
  fprintf(fp, "\t\"arm\":%d,\n", mature_mirna->arm);

  fprintf(fp, "\t\"star_sequence\":\"");
  for (u32 i = star_mirna->start; i < star_mirna->end; i++) {
    fprintf(fp, "%c", cand->sequence[i]);
  }
  fprintf(fp, "\",\n");
  fprintf(fp, "\t\"star_start\":%lld,\n", cand->start + star_mirna->start);
  fprintf(fp, "\t\"star_end\":%lld,\n", cand->start + star_mirna->end);
  fprintf(fp, "\t\"mfe\":\"%7.5lf kcal/mol/bp\",\n", cand->mfe);
  fprintf(fp, "\t\"paired_fraction\":%7.5lf,\n", cand->paired_fraction);
  fprintf(fp, "\t\"mfe_mean\":%7.5lf,\n", cand->mean);
  fprintf(fp, "\t\"mfe_sd\":%7.5lf,\n", cand->sd);
  fprintf(fp, "\t\"mfe_precise\":%8lf,\n", cand->mfe);
  fprintf(fp, "\t\"z_value\":%7.5lf,\n", (cand->mfe - cand->mean) / cand->sd);
  fprintf(fp, "\t\"p_value\":%7.5le,\n", cand->pvalue);
  fprintf(fp, "}");
  return E_SUCCESS;
}

int inititalize_html_report(FILE *fp) {
  if (fp == NULL) {
    return E_FILE_WRITING_FAILED;
  }
  fprintf(
      fp,
      "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'><"
      "meta http-equiv='X-UA-Compatible' content='IE=edge'><meta name='"
      "viewport' content='width=device-width,initial-scale=1'><title>"
      "miRA candidate summary</"
      "title><!--Bootstrap--><link rel='stylesheet' href='https://"
      "maxcdn.bootstrapcdn.com/bootstrap/3.3.2/css/"
      "bootstrap.min.css'></head><body><h1>miRA candidate summary</"
      "h1><table class='table table-bordered'><thead><tr><th><span>"
      "CandidateID<span class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></th><th><span>Chromosome<span "
      "class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></"
      "th><th><span>Strand<span class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></th><th><span>Start(pre)<span "
      "class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></"
      "th><th><span>Stop(pre)<span class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></th><th><span>MFE/nt<span "
      "class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></"
      "th><th><span>p-value<span class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></th><th><span>paired fraction<span "
      "class='glyphicon glyphicon-chevron-down' aria-hidden='true'></"
      "span></th><th><span>Mature sequence<span class='glyphicon "
      "glyphicon-chevron-down' aria-hidden='true'></span></span></"
      "th><th><span>Start(mature)<span class='glyphicon "
      "glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></th><th><span>Stop(mature)<span "
      "class='glyphicon glyphicon-chevron-down' aria-hidden='true'></"
      "span></th><th><span>Arm<span class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></th><th><span>Star sequence<span "
      "class='glyphicon glyphicon-chevron-down' aria-hidden='true'></"
      "span></th><th><span>Dicer check<span class='glyphicon "
      "glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></th><th><span>Start(star)<span "
      "class='glyphicon "
      "glyphicon-chevron-down' aria-hidden='true'></span></span></"
      "th><th><span>Stop(star)<span class='glyphicon glyphicon-chevron-down' "
      "aria-hidden='true'></span></span></th></tr></thead><tbody>");
  return E_SUCCESS;
}
int write_html_table_row(FILE *fp, struct extended_candidate *ecand) {
  if (fp == NULL) {
    return E_FILE_WRITING_FAILED;
  }
  struct micro_rna_candidate *cand = ecand->cand;
  struct candidate_subsequence_list *css_list = ecand->possible_micro_rnas;
  struct candidate_subsequence *mature_mirna = NULL;
  struct candidate_subsequence *star_mirna = NULL;

  for (size_t i = 0; i < css_list->n; i++) {
    mature_mirna = css_list->mature_sequences[i];
    star_mirna = mature_mirna->matching_sequence;

    fprintf(fp,
            "<tr><td ><a "
            "href='reports/Cluster_%lld_report.pdf'>precursor_%lld_%ld_%s</a></"
            "td><td>%s</td><td>%s</td><td>%lld</"
            "td><td>%lld</td><td>%7.5lf "
            "</td><td>%7.5le</td><td>%7.5lf</td><td>",
            cand->id, cand->id, i, (cand->strand == '-') ? "minus" : "plus",
            cand->chrom, (cand->strand == '-') ? "minus" : "plus", cand->start,
            cand->end, cand->mfe, cand->pvalue, cand->paired_fraction);
    for (u32 i = mature_mirna->start; i < mature_mirna->end; i++) {
      fprintf(fp, "%c", cand->sequence[i]);
    }
    fprintf(fp, "</td><td>%lld</td><td>%lld</td><td>%dp</td><td>",
            cand->start + mature_mirna->start, cand->start + mature_mirna->end,
            mature_mirna->arm);
    for (u32 i = star_mirna->start; i < star_mirna->end; i++) {
      fprintf(fp, "%c", cand->sequence[i]);
    }
    fprintf(fp, "</td><td>%s</td><td>%lld</td><td>%lld</td></tr>",
            (star_mirna->is_artificial) ? "fail" : "success",
            cand->start + star_mirna->start, cand->start + star_mirna->end);
  }

  return E_SUCCESS;
}

int finalize_html_report(FILE *fp) {
  if (fp == NULL) {
    return E_FILE_WRITING_FAILED;
  }
  fprintf(fp,
          "</tbody></"
          "table><!--jQuery(necessaryforBootstrap'sJavaScriptplugins)--><"
          "script src='https://code.jquery.com/jquery-1.11.2.min.js'></"
          "script><!--Includeallcompiledplugins(below),"
          "orincludeindividualfilesasneeded--><script src='https://"
          "maxcdn.bootstrapcdn.com/bootstrap/3.3.2/js/bootstrap.min.js'></"
          "script><script>$(function() {  function get_comp_func(index, dir) { "
          "   return function(a, b) {      var el1 = "
          "$(a).find('td').get(index).innerHTML;      var el2 = "
          "$(b).find('td').get(index).innerHTML;      tmp1 = parseFloat(el1);  "
          "    tmp2 = parseFloat(el2);      if (isNaN(tmp1) || isNaN(tmp2)) {  "
          "      var result = "
          "el1.toLowerCase().localeCompare(el2.toLowerCase());        return "
          "dir * result;      } else {        if (tmp1 > tmp2) {          "
          "return dir;        } else {          return -dir;        }      }   "
          " }  }  function sort_rows(th, index, dir) {    var dir = 1;    var "
          "icon = $(th).find('.glyphicon');    if ($(icon).is(':visible')) {   "
          "   if ($(icon).hasClass('glyphicon-chevron-down')) {        "
          "$(icon).removeClass('glyphicon-chevron-down');        "
          "$(icon).addClass('glyphicon-chevron-up');        dir = -1      } "
          "else {        $(icon).removeClass('glyphicon-chevron-up');        "
          "$(icon).addClass('glyphicon-chevron-down');        dir = 1;      }  "
          "  } else {      $('.glyphicon').hide();      $(icon).show();    }   "
          " var rows = $('tr').slice(1);    rows.sort(get_comp_func(index, "
          "dir));    for (var i = 0; i < rows.length; i++) {      "
          "rows[i].parentNode.appendChild(rows[i]);    }  }  "
          "$('.glyphicon').hide();  $('th').each(function(index) {    "
          "$(this).click(function() { sort_rows(this, index, 1); });  "
          "})})</script></body></html>");
  return E_SUCCESS;
}

int cleanup_auxiliary_files(char *cov_plot_file, char *structure_file,
                            char *coverage_file, char *tex_file,
                            struct configuration_params *config) {
  if (config->cleanup_auxiliary_files != 0) {
    if (cov_plot_file != NULL) {
      remove(cov_plot_file);
    }
    if (structure_file != NULL) {
      remove(structure_file);
    }
    if (coverage_file != NULL) {
      remove(coverage_file);
    }
    if (tex_file != NULL) {
      remove(tex_file);
      char *dot = strchr(tex_file, '.');
      if (dot != NULL) {
        *dot = 0;
        char tmp_file[1024];
        sprintf(tmp_file, "%s.aux", tex_file);
        remove(tmp_file);
        sprintf(tmp_file, "%s.log", tex_file);
        remove(tmp_file);
      }
    }
  }

  if (cov_plot_file != NULL) {
    free(cov_plot_file);
  }
  if (structure_file != NULL) {
    free(structure_file);
  }
  if (coverage_file != NULL) {
    free(coverage_file);
  }
  if (tex_file != NULL) {
    free(tex_file);
  }
  return E_SUCCESS;
}
