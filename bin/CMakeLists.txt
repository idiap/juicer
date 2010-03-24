set(SCRIPTS
  aux2eps.pl
  build-wfst
  build-wfst-mit
  build-wfst-openfst
  do-star-closure.pl
  fstRemoveAux.pl
  map-labels.pl
  logical2physical.pl
  untieModels.sh
)

install(
  PROGRAMS ${SCRIPTS}
  DESTINATION bin
)
