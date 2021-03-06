vvflow(1) -- solve the CFD problem
====

## SYNOPSIS

`vvflow` [<OPTIONS>] <FILE>

## DESCRIPTION

vvflow is the main tool of Vvflow CFD Suite.
It performs the simulation and solves the CFD problem.

Problem statement is loaded from <FILE>, which can generated by vvcompose(1) tool,
or copied from another vvflow simulation results.

## OPTIONS

  * -v, --version :
    print program version and exit

  * -h, --help :
    print help and exit

  * --progress :
    print interactive progress printing during the simulation

  * --profile :
    save pressure and friction profiles along bodies surfaces to the stepdata file

## SEE ALSO
  vvcompose(1), vvxtract(1), vvplot(1)
