
SUBDIRS =	include \
		src

ChangeLog:
	@echo -n "Generating ChangeLog ... "
	@./gitlog2changelog.sh -r "v.*" -o "$(top_builddir)/ChangeLog"
	@echo "ok"

.PHONY: ChangeLog

EXTRA_DIST = ChangeLog
