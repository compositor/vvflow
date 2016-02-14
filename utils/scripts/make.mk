TARGETS_INSTALL   += vvscripts_install
TARGETS_UNINSTALL += vvscripts_uninstall

vvscripts_install: avg.awk movavg.awk stdev.awk vvencode vvgen_plate | $(PREFIX)/bin
	cp $^ $(PREFIX)/bin

vvscripts_uninstall:
	rm -f $(PREFIX)/bin/avg.awk
	rm -f $(PREFIX)/bin/movavg.awk
	rm -f $(PREFIX)/bin/stdev.awk
	rm -f $(PREFIX)/bin/vvencode
	rm -f $(PREFIX)/bin/vvgen_plate

