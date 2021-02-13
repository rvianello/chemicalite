#ifndef CHEMICALITE_SETTINGS_INCLUDED
#define CHEMICALITE_SETTINGS_INCLUDED

int chemicalite_init_settings(sqlite3 *db);

enum ChemicaliteSetting {
  LOGGING,
#ifdef ENABLE_TEST_SETTINGS
  ANSWER,
  PI,
#endif
  CHEMICALITE_NUM_SETTINGS
};
typedef enum ChemicaliteSetting ChemicaliteSetting;

enum ChemicaLiteOption {
  LOGGING_DISABLED, LOGGING_STDOUT, LOGGING_STDERR,
  CHEMICALITE_NUM_OPTIONS
};
typedef enum ChemicaLiteOption ChemicaLiteOption;

const char * chemicalite_option_label(ChemicaLiteOption option);

int chemicalite_set_option(ChemicaliteSetting setting, ChemicaLiteOption value);
int chemicalite_get_option(ChemicaliteSetting setting, ChemicaLiteOption *pValue);
int chemicalite_set_int(ChemicaliteSetting setting, int value);
int chemicalite_get_int(ChemicaliteSetting setting, int *pValue);
int chemicalite_set_double(ChemicaliteSetting setting, double value);
int chemicalite_get_double(ChemicaliteSetting setting, double *pValue);

#endif
