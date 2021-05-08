#ifndef CHEMICALITE_SETTINGS_INCLUDED
#define CHEMICALITE_SETTINGS_INCLUDED

int chemicalite_init_settings(sqlite3 *db);

enum ChemicaLiteSetting {
  LOGGING,
#ifdef ENABLE_TEST_SETTINGS
  ANSWER,
  PI,
#endif
  CHEMICALITE_NUM_SETTINGS
};
typedef enum ChemicaLiteSetting ChemicaLiteSetting;

enum ChemicaLiteOption {
  LOGGING_DISABLED, LOGGING_STDOUT, LOGGING_STDERR,
  CHEMICALITE_NUM_OPTIONS
};
typedef enum ChemicaLiteOption ChemicaLiteOption;

const char * chemicalite_option_label(ChemicaLiteOption option);

int chemicalite_set(ChemicaLiteSetting setting, ChemicaLiteOption value);
int chemicalite_get(ChemicaLiteSetting setting, ChemicaLiteOption *pValue);
int chemicalite_set(ChemicaLiteSetting setting, int value);
int chemicalite_get(ChemicaLiteSetting setting, int *pValue);
int chemicalite_set(ChemicaLiteSetting setting, double value);
int chemicalite_get(ChemicaLiteSetting setting, double *pValue);

#endif
