
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 * 
 */

#include <auto/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>
#include <turglem/lemmatizer.h>
#include <turglem/russian/adapter.h>

#define TRUS_DICT TURGLEM_INSTALL_PREFIX "/share/turglem/russian/dict_russian.auto"
#define TRUS_PRED TURGLEM_INSTALL_PREFIX "/share/turglem/russian/prediction_russian.auto"
#define TRUS_PARA TURGLEM_INSTALL_PREFIX "/share/turglem/russian/paradigms_russian.bin"

#define TRUS_SHOW_NFORM    (1 << 0)
#define TRUS_SHOW_PARADIGM (1 << 1)
#define TRUS_SHOW_GRAMMEM  (1 << 2)

typedef struct trus_getopt
    trus_getopt_t;

struct trus_getopt
{
    uint32_t show;
    int no_prediction;
};

static struct option options [] =
{
    { "nform"         , 0, NULL, 'n' },
    { "paradigm"      , 0, NULL, 'p' },
    { "grammem"       , 0, NULL, 'g' },
    { "no-prediction" , 0, NULL, 'P' },
    {  NULL           , 0, NULL,  0  },
};

int trus_getopt_usage(int argc, char **argv)
{
    fprintf(stderr,

        "Usage:                                          \n"
        "  %s -[P]n [WORD]...                            \n"
        "  %s -[P]p [WORD]...                            \n"
        "  %s -g [GRAMMEM]...                            \n"
        "                                                \n"
        "Options:                                        \n"
        "  -n, --nform         : show normal form only   \n"
        "  -p, --paradigm      : show the whole paradigm \n"
        "  -g, --grammem       : show words with grammem \n"
        "                                                \n"
        "  -P, --no-prediction : don't use prediction    \n"
        "                                                \n"

    , argv[0], argv[0], argv[0]);

    return 0;
}

int trus_getopt_parse(int argc, char **argv, trus_getopt_t *opts)
{
    int c;

    while (-1 != (c = getopt_long(argc, argv, "npgP", options, NULL)))
    {
        switch (c)
        {
            case 'n': opts->show |= TRUS_SHOW_NFORM;    break;
            case 'p': opts->show |= TRUS_SHOW_PARADIGM; break;
            case 'g': opts->show |= TRUS_SHOW_GRAMMEM;  break;

            case 'P': opts->no_prediction = 1; break;

            default:
                return -1;
        }
    }

    return 0;
}

static int lemmatize_and_print(turglem lem, const char *s, size_t sz_s, trus_getopt_t *opts)
{
    int form [2 * 1024];
    MAFSA_letter letter_src [1024];
    ssize_t sz_letter_src;
    size_t sz_form, i;

    if (0 > (sz_letter_src = RUSSIAN_conv_binary_to_letter_utf8(s,
        sz_s, letter_src, 1024)))
    {
        return -1;
    }

    sz_form = turglem_lemmatize(lem, letter_src, sz_letter_src, form,
        1024, RUSSIAN_LETTER_DELIM, !opts->no_prediction);

    for (i = 0; i < sz_form; ++i)
    {
        size_t j;
        size_t fsz;

        if (opts->show & TRUS_SHOW_NFORM)
        {
            MAFSA_letter letter_dst [1024];
            char binary_dst [1024];

            size_t sz_letter_dst;
            size_t sz_binary_dst;

            sz_letter_dst = turglem_build_form(lem, letter_src, sz_letter_src,
                letter_dst, 1024, form[2*i], form[2*i+1], 0);

            sz_binary_dst = RUSSIAN_conv_letter_to_binary_utf8(letter_dst,
                sz_letter_dst, binary_dst, 1024);

            printf("%.*s%c", (int) sz_binary_dst, binary_dst,
                i < sz_form - 1 ? '|' : '\n');
        }

        if (opts->show & TRUS_SHOW_PARADIGM)
        {
            fsz = turglem_paradigms_get_form_count(lem->paradigms, form[2*i]);

            /* printf("\033[1mPARADIGM  FORM                                                       GRAMMEM  POS\033[m\n"); */

            for (j = 0; j < fsz; ++j)
            {
                MAFSA_letter letter_dst [1024];
                char binary_dst [1024];

                size_t sz_letter_dst;
                size_t sz_binary_dst;

                uint64_t grm;
                uint8_t  pos;

                size_t k;
                size_t grm_pos;
                  char grm_str [256];

                grm = turglem_paradigms_get_grammem(lem->paradigms, form[2*i], j);
                pos = turglem_paradigms_get_part_of_speech(lem->paradigms, form[2*i], j);

                sz_letter_dst = turglem_build_form(lem, letter_src, sz_letter_src,
                    letter_dst, 1024, form[2*i], form[2*i+1], j);

                sz_binary_dst = RUSSIAN_conv_letter_to_binary_utf8(letter_dst,
                    sz_letter_dst, binary_dst, 1024);

                grm_pos = snprintf(grm_str, 256, ",");

                for (k = 0; k < 64; ++k)
                {
                    if (grm & (1ULL << k))
                    {
                        grm_pos += snprintf(grm_str + grm_pos, 256 - grm_pos,
                            "%"PRIuMAX",", (uintmax_t) k);
                    }
                }

                printf("%8d  %4"PRIuMAX"  %s  %3"PRIu8"  %.*s %s\n", form[2*i], (uintmax_t) j, grm_str,
                    pos, (int) sz_binary_dst, binary_dst, (form[2*i+1] == j) ? "*" : " ");
            }
        }
    }

    return 0;
}

turglem lem;

void check_grammem_and_print(void *user_data, const MAFSA_letter *l, size_t sz_l)
{
    int form [2];

    MAFSA_automaton_str_to_int_pair(l, sz_l, RUSSIAN_LETTER_DELIM, form);

    uint64_t grm = turglem_paradigms_get_grammem(lem->paradigms, form[0], form[1]);

    uint64_t *grm_mask = (uint64_t *) user_data;

    if ((grm & *grm_mask) == *grm_mask)
    {
        char binary_dst [1024];
        size_t sz_binary_dst;

        sz_binary_dst = RUSSIAN_conv_letter_to_binary_utf8(l, sz_l, binary_dst, 1024);

        char *binary_dst_end = memchr(binary_dst, '|', sz_binary_dst);
        if (NULL != binary_dst_end) sz_binary_dst = binary_dst_end - binary_dst;

        uint8_t pos;

        pos = turglem_paradigms_get_part_of_speech(lem->paradigms, form[0], form[1]);

        size_t k;
        size_t grm_pos;
          char grm_str [256];

        grm_pos = snprintf(grm_str, 256, ",");

        for (k = 0; k < 64; ++k)
        {
            if (grm & (1ULL << k))
            {
                grm_pos += snprintf(grm_str + grm_pos, 256 - grm_pos,
                    "%"PRIuMAX",", (uintmax_t) k);
            }
        }

        printf("%8d  %4"PRIuMAX"  %s  %3"PRIu8"  %.*s\n", form[0], (uintmax_t) form[1],
            grm_str, pos, (int) sz_binary_dst, binary_dst);
    }
}

int main(int argc, char **argv)
{
    int err_no;
    int err_what;
    int i;

    trus_getopt_t opts;
    memset(&opts, 0, sizeof(opts));

    if (0 != trus_getopt_parse(argc, argv, &opts))
    {
        trus_getopt_usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    if (0 == opts.show)
    {
        trus_getopt_usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    lem = turglem_load(TRUS_DICT, TRUS_PRED, TRUS_PARA, &err_no, &err_what);

    if (0 != err_no)
    {
        fprintf(stderr, "turglem_load("TRUS_DICT", "TRUS_PRED", "TRUS_PARA", ...):"
            " %d: %s", err_no, turglem_error_what_string(err_what));

        exit(EXIT_FAILURE);
    }

    if (opts.show & TRUS_SHOW_GRAMMEM)
    {
        uint64_t grm_mask = 0ULL;

        for (i = optind; i < argc; ++i)
        {
            grm_mask |= (1ULL << atoi(argv[i]));
        }

        MAFSA_letter tmp [1024];

        MAFSA_automaton_enumerate(lem->words, NULL, 0, tmp, 1024,
            &grm_mask, check_grammem_and_print);

        goto end;
    }

    for (i = optind; i < argc; ++i)
    {
        lemmatize_and_print(lem, argv[i], strlen(argv[i]), &opts);
    }

    if (argc > optind)
    {
        exit(EXIT_SUCCESS);
    }

    while (!feof(stdin) && !ferror(stdin))
    {
        char word [1024];
        size_t sz;

        if (NULL == fgets(word, 1024, stdin))
        {
            continue;
        }

        sz = strcspn(word, "\r\n");
        word[sz] = 0;

        lemmatize_and_print(lem, word, sz, &opts);
    }

end:
    turglem_close(lem);

    return 0;
}

