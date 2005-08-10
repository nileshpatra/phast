#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <misc.h>
#include <msa.h>
#include <sufficient_stats.h>
#include <numerical_opt.h>
#include <tree_model.h>
#include <fit_em.h>
#include <time.h>

void usage(char *prog) {
  printf("\n\
PROGRAM: %s\n\
\n\
DESCRIPTION:\n\
\n\
    Estimate errors in model parameters using parameteric or\n\
    non-parameteric boostrapping.  The tree topology is not inferred --\n\
    estimated errors are conditional on the given topology.\n\
\n\
USAGE: %s [OPTIONS] <model_fname>|<msa_fname>\n\
\n\
    If a model is given in the form of a .mod file (<model_fname>),\n\
    then parameteric bootstrapping is performed, using synthetic data\n\
    sets drawn from the distribution that is defined by the model.\n\
    Otherwise, the input file is assumed to be a multiple alignment,\n\
    and non-parameteric bootstrapping is performed (resampling of\n\
    sites in alignment).  Output is to stdout and consists of a line\n\
    for each parameter, with columns for the mean, standard deviation\n\
    (approximate standard error), median, minimum, and maximum of\n\
    estimates, plus the boundaries of 95%% and 90%% confidence\n\
    intervals.\n\
\n\
    See usage for phyloFit for additional details on tree-building\n\
    options.\n\
\n\
OPTIONS:\n\
\n\
 (bootstrapping options)\n\
\n\
    --nsites, -L <number>\n\
        Number of sites in sampled alignments.  If an alignment is\n\
        given (non-parametric case), default is number of sites in\n\
        alignment, otherwise default is 1000.\n\
\n\
    --nreps, -n <number>\n\
        Number of replicates.  Default is 100.\n\
\n\
    --msa-format, -i FASTA|PHYLIP|MPM|SS\n\
        (non-parameteric case only)  Alignment format.  Default is FASTA.\n\
\n\
    --dump-mods, -d <fname_root>\n\
        Dump .mod files for individual estimated models (one for each\n\
        replicate).\n\
\n\
    --dump-samples, -m <fname_root>\n\
        Dump the raw alignments that are generated by simulation\n\
        (parameteric case) or by resampling (nonparameteric case). \n\
\n\
    --no-estimates, -x \n\
        Don't estimate model parameters or report statistics.  Can be\n\
        used with --dump-samples to create data sets that can be\n\
        processed separately, e.g., in parallel on a compute cluster.\n\
\n\
    --read-mods, -R <fname_list>\n\
        Read estimated models from list of filenames instead of\n\
        generating alignments and estimating parameters. Can be used\n\
        to run the statistics for replicates processed separately (see\n\
        --dump-samples).  When this option is used, the primary\n\
        argument to the program (<model_fname>|<msa_fname>) will be\n\
        ignored.\n\
\n\
    --output-average, -A <fname>\n\
        Output a tree model representing the average of all input\n\
        models to the specified file.\n\
\n\
    --quiet, -q\n\
        Proceed quietly.\n\
\n\
    --help, -h\n\
        Print this help message.\n\
\n\
 (tree-building options)\n\
\n\
    --tree, -t <tree_fname>|<tree_string>\n\
        (Required if non-parameteric and more than two species) Name\n\
        of file or literal string defining tree topology.\n\
\n\
    --subst-mod, -s JC69|F81|HKY85|REV|UNREST|R2|R2S|U2|U2S|R3|R3S|U3|U3S\n\
        (default REV).  Nucleotide substitution model.\n\
\n\
    --nrates, -k <nratecats>\n\
        (default 1).  Number of rate categories to use.  Specifying a\n\
        value of greater than one causes the discrete gamma model for\n\
        rate variation to be used.\n\
\n\
    --EM, -E\n\
        Use EM rather than the BFGS quasi-Newton algorithm for parameter\n\
        estimation.\n\
\n\
    --precision, -p HIGH|MED|LOW\n\
        (default HIGH) Level of precision to use in estimating model\n\
        parameters.\n\
\n\
    --init-model, -M <mod_fname>\n\
        Initialize optimization procedure with specified tree model.\n\
\n\
    --init-random, -r\n\
        Initialize parameters randomly.\n\n", prog, prog);
  exit(0);
}

/* attempt to provide a brief description of each estimated parameter,
   based on a given TreeModel definition */
void set_param_descriptions(char **descriptions, TreeModel *mod) {
  List *traversal;
  String *str = str_new(STR_MED_LEN);
  int nparams = tm_get_nparams(mod);
  int nrv_params = tm_get_nratevarparams(mod);
  int nrm_params = tm_get_nratematparams(mod);
  int i, j, idx = 0;

  assert(mod->estimate_branchlens == TM_BRANCHLENS_ALL);
  assert(mod->estimate_backgd == FALSE);

  traversal = tr_preorder(mod->tree);
  for (i = 0; i < lst_size(traversal); i++) {
    TreeNode *n = lst_get_ptr(traversal, i);
    if (n->parent == NULL) continue;
    /* if the model is reversible, then the first parameter is
       the sum of the lengths of the two branches from the root */
    if (n == mod->tree->lchild && tm_is_reversible(mod->subst_mod))
      sprintf(descriptions[idx++], "branch (spans root)"); 
    else if (n != mod->tree->rchild || !tm_is_reversible(mod->subst_mod)) {
      if (strlen(n->name) > 0)
        sprintf(descriptions[idx++], "branch (lf_%s->anc_%d)", n->name, n->parent->id);
      else 
        sprintf(descriptions[idx++], "branch (anc_%d->anc_%d)", n->id, n->parent->id);
    }
  }

  for (i = 0; i < nrv_params; i++) {
    if (nrv_params == 1) 
      strcpy(descriptions[idx++], "alpha");
    else
      sprintf(descriptions[idx++], "rate var #%d", i+1);
  }

  if (nrm_params == 1)
    strcpy(descriptions[idx++], "kappa");
  else {
    for (i = 0; i < nrm_params; i++) {
      List *rows = mod->rate_matrix_param_row[idx];
      List *cols = mod->rate_matrix_param_col[idx];
      str_cpy_charstr(str, "rmatrix");
      for (j = 0; j < lst_size(rows); j++) {
        char tmp[STR_SHORT_LEN];
        sprintf(tmp, " (%d,%d)", lst_get_int(rows, j) + 1, 
                lst_get_int(cols, j) + 1);
        str_append_charstr(str, tmp);
      }
      strcpy(descriptions[idx++], str->chars);
    }
  }
  assert(idx == nparams);
  str_free(str);
}

int main(int argc, char *argv[]) {
  
  /* variables for args with default values */
  int default_nsites = 1000, nsites = -1, nreps = 100, input_format = FASTA, 
    subst_mod = REV, nrates = 1, precision = OPT_HIGH_PREC;
  int quiet = FALSE, use_em = FALSE, random_init = FALSE, parameteric = FALSE, 
    do_estimates = TRUE;
  TreeNode *tree = NULL;
  TreeModel *model = NULL, *init_mod = NULL;
  MSA *msa;
  char *dump_mods_root = NULL, *dump_msas_root = NULL, *ave_model = NULL;
  TreeModel **input_mods = NULL;

  /* other variables */
  FILE *INF, *F;
  char c;
  int i, j, opt_idx, nparams = -1;
  String *tmpstr;
  HMM *hmm = NULL;
  List **estimates;
  double *p = NULL;
  int *tmpcounts;
  char **descriptions = NULL;
  List *tmpl;
  char fname[STR_MED_LEN];
  char tmpchstr[STR_MED_LEN];
  TreeModel *repmod = NULL;

  struct option long_opts[] = {
    {"nsites", 1, 0, 'L'},
    {"nreps", 1, 0, 'n'},
    {"msa-format", 1, 0, 'i'},
    {"dump-mods", 1, 0, 'd'},
    {"dump-samples", 1, 0, 'm'},
    {"no-estimates", 0, 0, 'x'},
    {"read-mods", 1, 0, 'R'},
    {"output-average", 1, 0, 'A'},
    {"quiet", 0, 0, 'q'},
    {"help", 0, 0, 'h'},
    {"tree", 1, 0, 't'},
    {"subst-mod", 1, 0, 's'},
    {"nrates", 1, 0, 'k'},
    {"EM", 0, 0, 'E'},
    {"precision", 1, 0, 'p'},
    {"init-model", 1, 0, 'M'},
    {"init-random", 0, 0, 'r'},
    {0, 0, 0, 0}
  };

  while ((c = getopt_long(argc, argv, "L:n:i:d:m:xR:qht:s:k:Ep:M:r", 
                          long_opts, &opt_idx)) != -1) {
    switch (c) {
    case 'L':
      nsites = get_arg_int_bounds(optarg, 10, INFTY);
      break;
    case 'n':
      if (input_mods != NULL) die("ERROR: Can't use --nreps with --read-mods.\n");
      nreps = get_arg_int_bounds(optarg, 1, INFTY);
      break;
    case 'i':
      input_format = msa_str_to_format(optarg);
      if (input_format == -1)
        die("ERROR: unrecognized alignment format.  Type 'phyloBoot -h' for usage.\n");
      break;
    case 'd':
      dump_mods_root = optarg;
      break;
    case 'm':
      dump_msas_root = optarg;
      break;
    case 'x':
      do_estimates = FALSE;
      break;
    case 'R':
      tmpl = get_arg_list(optarg);
      nreps = lst_size(tmpl);
      input_mods = smalloc(nreps * sizeof(void*));
      for (i = 0; i < lst_size(tmpl); i++) {
        FILE *F = fopen_fname(((String*)lst_get_ptr(tmpl, i))->chars, "r");
        input_mods[i] = tm_new_from_file(F);
        fclose(F);
      }
      lst_free_strings(tmpl); lst_free(tmpl);
      break;
    case 'A':
      ave_model = optarg;
      break;
    case 'q':
      quiet = 1;
      break;
    case 'h':
      usage(argv[0]);
    case 't':
      if (optarg[0] == '(')     /* in this case, assume topology given
                                   at command line */
        tree = tr_new_from_string(optarg);
      else 
        tree = tr_new_from_file(fopen_fname(optarg, "r"));
      break;
    case 's':
      subst_mod = tm_get_subst_mod_type(optarg);
      if (subst_mod == UNDEF_MOD) 
        die("ERROR: illegal substitution model.  Type \"phyloBoot -h\" for usage.\n");
      break;
    case 'k':
      nrates = get_arg_int_bounds(optarg, 1, INFTY);
      break;
    case 'E':
      use_em = 1;
      break;
    case 'p':
      if (!strcmp(optarg, "LOW")) precision = OPT_LOW_PREC;
      else if (!strcmp(optarg, "MED")) precision = OPT_MED_PREC;
      else if (!strcmp(optarg, "HIGH")) precision = OPT_HIGH_PREC;
      else die("ERROR: --precision must be LOW, MED, or HIGH.\n\n");
      break;
    case 'M':
      init_mod = tm_new_from_file(fopen_fname(optarg, "r"));
      break;
    case 'r':
      random_init = 1;
      break;
    case '?':
      die("Bad argument.  Try '%s -h'.\n", argv[0]);
    }
  }

  srandom(time(NULL));

  if (input_mods == NULL) {     /* only do if models aren't given */
    if (optind != argc - 1) 
      die("Input filename required.  Try '%s -h'.\n", argv[0]);

    INF = fopen_fname(argv[optind], "r");

    tmpstr = str_new_charstr(argv[optind]);
    if (str_ends_with_charstr(tmpstr, ".mod")) {
      parameteric = TRUE;
      model = tm_new_from_file(INF);
      tree = model->tree;
    }
    else
      msa = msa_new_from_file(INF, input_format, NULL);

    /* general set up -- different for parameteric and non-parameteric cases */
    if (!parameteric) {
      if (tree == NULL) {
        if (msa->nseqs == 2) {
          sprintf(tmpchstr, "(%s,%s)", msa->names[0], msa->names[1]);
          tree = tr_new_from_string(tmpchstr);
        }
        else if (msa->nseqs == 3 && tm_is_reversible(subst_mod)) {
          sprintf(tmpchstr, "(%s,(%s,%s))", msa->names[0], msa->names[1], 
                  msa->names[2]);
          tree = tr_new_from_string(tmpchstr);
        }
        else if (do_estimates)
          die("ERROR: must specify tree topology.\n");
      }
      else if (msa->nseqs * 2 - 1 != tree->nnodes)
        die("ERROR: Tree must have 2n-1 nodes, where n is the number of sequences in the\nalignment.  Even with a reversible model, specify a rooted tree; the root\nwill be ignored in the optimization procedure.\n");

      msa_remove_N_from_alph(msa); /* for backward compatibility */

      if (nsites == -1) nsites = msa->length;

      /* define probability vector from tuple counts */
      if (msa->ss == NULL)
        ss_from_msas(msa, tm_order(subst_mod) + 1, FALSE, NULL, NULL, NULL, -1);
      p = smalloc(msa->ss->ntuples * sizeof(double));
      for (i = 0; i < msa->ss->ntuples; i++) p[i] = msa->ss->counts[i];
      normalize_probs(p, msa->ss->ntuples);
      tmpcounts = smalloc(msa->ss->ntuples * sizeof(int));
    }
    else {                        /* parameteric */
      /* create trivial HMM, for use in tm_generate_msa */
      hmm = hmm_create_trivial();
      if (nsites == -1) nsites = default_nsites;
    }
  } /* if input_mods == NULL */

  for (i = 0; i < nreps; i++) {
    Vector *params;
    TreeModel *thismod;

    /* generate alignment */
    if (input_mods == NULL) {   /* skip if models given */
      if (parameteric) 
        msa = tm_generate_msa(nsites, hmm->transition_matrix, &model, NULL);
      else {
        mn_draw(nsites, p, msa->ss->ntuples, tmpcounts);
                                /* here we simply redraw numbers of
                                   tuples from multinomial distribution
                                   defined by orig alignment */
	                        /* WARNING: current implementation is
				   inefficient for large nsites*ntuples */
        for (j = 0; j < msa->ss->ntuples; j++) msa->ss->counts[j] = tmpcounts[j];
                                /* (have to convert from int to double) */
        msa->length = nsites;
      }

      if (dump_msas_root != NULL) {
        sprintf(fname, "%s.%d.ss", dump_msas_root, i+1);
        if (!quiet) fprintf(stderr, "Dumping alignment to %s...\n", fname);
        F = fopen_fname(fname, "w+");
        if (msa->ss == NULL) 
          ss_from_msas(msa, tm_order(subst_mod) + 1, FALSE, NULL, NULL, NULL, -1);
        ss_write(msa, F, FALSE);
        fclose(F);
      }
    }

    /* now estimate model parameters */
    if (input_mods == NULL && do_estimates) {
      if (init_mod == NULL) 
        thismod = tm_new(tr_create_copy(tree), NULL, NULL, subst_mod, 
                         msa->alphabet, nrates, 1, NULL, -1);
      else {
        thismod = tm_create_copy(init_mod);  
        tm_reinit(thismod, subst_mod, nrates, thismod->alpha, NULL, NULL);
      }

      if (random_init) 
        params = tm_params_init_random(thismod);
      else if (init_mod != NULL)
        params = tm_params_new_init_from_model(init_mod);
      else
        params = tm_params_init(thismod, .1, 5, 1);    

      if (init_mod != NULL && thismod->backgd_freqs != NULL) {
        vec_free(thismod->backgd_freqs);
        thismod->backgd_freqs = NULL; /* force re-estimation */
      }

      if (!quiet) 
        fprintf(stderr, "Estimating model for replicate %d of %d...\n", i+1, nreps);

      if (use_em)
        tm_fit_em(thismod, msa, params, -1, precision, NULL);
      else
        tm_fit(thismod, msa, params, -1, precision, NULL);

      if (dump_mods_root != NULL) {
        sprintf(fname, "%s.%d.mod", dump_mods_root, i+1);
        if (!quiet) fprintf(stderr, "Dumping model to %s...\n", fname);
        F = fopen_fname(fname, "w+");
        tm_print(F, thismod);
        fclose(F);
      }
    } 

    else if (input_mods != NULL) { 
      /* in this case, we need to set up a parameter vector from
         the input model */
      thismod = input_mods[i];
      params = tm_params_new_init_from_model(thismod);
      if (nparams > 0 && params->size != nparams)
        die("ERROR: input models have different numbers of parameters.\n");
      if (repmod == NULL) repmod = thismod; /* keep around one representative model */
    }

    /* collect parameter estimates */
    if (do_estimates) {
      /* set up record of estimates; easiest to init here because number
         of parameters not always known above */
      if (nparams <= 0) {
        nparams = params->size;
        estimates = smalloc(nparams * sizeof(void*));
        descriptions = smalloc(nparams * sizeof(char*));
        for (j = 0; j < nparams; j++) {
          estimates[j] = lst_new_dbl(nreps);
          descriptions[j] = smalloc(STR_MED_LEN * sizeof(char));
        }
        set_param_descriptions(descriptions, thismod);
      }

      /* record estimates for this replicate */
      for (j = 0; j < nparams; j++)
        lst_push_dbl(estimates[j], vec_get(params, j));
    }

    if (input_mods == NULL && do_estimates) {
      if (repmod == NULL) repmod = thismod; /* keep around one representative model */
      else tm_free(thismod);
    }
    if (do_estimates) vec_free(params);
  }

  /* finally, compute and print stats */
  if (do_estimates) {
    Vector *ave_params = vec_new(nparams); 
    printf("%-7s %-25s %9s %9s %9s %9s %9s %9s %9s %9s %9s\n", "param", 
           "description", "mean", "stdev", "median", "min", "max", "95%_min", 
           "95%_max", "90%_min", "90%_max");
    for (j = 0; j < nparams; j++) {
      double mean = lst_dbl_mean(estimates[j]);
      double stdev = lst_dbl_stdev(estimates[j]);
      double quantiles[] = {0, 0.025, 0.05, 0.5, 0.95, 0.975, 1};
      double quantile_vals[7]; 
      lst_qsort_dbl(estimates[j], ASCENDING);
      lst_dbl_quantiles(estimates[j], quantiles, 7, quantile_vals);

      printf("%-7d %-25s %9.5f %9.5f %9.5f %9.5f %9.5f %9.5f %9.5f %9.5f %9.5f\n", 
             j, descriptions[j], mean, stdev, quantile_vals[3], quantile_vals[0], 
             quantile_vals[6], quantile_vals[1], quantile_vals[5], quantile_vals[2], 
             quantile_vals[4]);
      vec_set(ave_params, j, mean);
    }

    if (ave_model != NULL) {
      tm_unpack_params(repmod, ave_params, -1);
      if (!quiet) fprintf(stderr, "Writing average model to %s...\n", ave_model);
      tm_print(fopen_fname(ave_model, "w+"), repmod);
    }
    vec_free(ave_params);
  }

  if (!quiet) fprintf(stderr, "Done.\n");
  return 0;
}
