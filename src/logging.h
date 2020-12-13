#ifndef CHEMICALITE_LOGGING_INCLUDED
#define CHEMICALITE_LOGGING_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

  void chemicalite_log(int iErrCode, const char *zFormat, ...);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif
