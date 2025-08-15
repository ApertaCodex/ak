# Makefile for ak (C++ API key manager)
# Build, install, and package (.deb / .rpm)
#
# Usage:
#   make                # build ./ak
#   sudo make install   # install to /usr/local/bin/ak
#   sudo make uninstall # remove installed binary
#   make package-deb    # builds ./dist/ak_<ver>_<arch>.deb (needs dpkg-deb)
#   make package-rpm    # builds ./dist/ak-<ver>-1.<arch>.rpm (needs rpmbuild or fpm)
#   make clean

APP       ?= ak
VERSION   ?= 0.1.0
# Detect arch name for packages
UNAME_M   := $(shell uname -m)
# Map to Debian/RPM arch tags
DEB_ARCH  := $(if $(filter $(UNAME_M),x86_64 amd64),amd64,$(UNAME_M))
RPM_ARCH  := $(if $(filter $(UNAME_M),x86_64 amd64),x86_64,$(UNAME_M))

CXX       ?= g++
CXXFLAGS  ?= -std=c++17 -O2 -pipe -Wall -Wextra -Iinclude -Itests/googletest/googletest/include -Itests/googletest/googlemock/include
LDFLAGS   ?=
PREFIX    ?= /usr/local
BINDIR    ?= $(PREFIX)/bin

# Source files
CORE_SRC  := src/core/config.cpp
CRYPTO_SRC := src/crypto/crypto.cpp
STORAGE_SRC := src/storage/vault.cpp
UI_SRC    := src/ui/ui.cpp
SYSTEM_SRC := src/system/system.cpp
CLI_SRC   := src/cli/cli.cpp
SERVICES_SRC := src/services/services.cpp
COMMANDS_SRC := src/commands/commands.cpp
MAIN_SRC  := src/main.cpp

APP_SRCS  := $(CORE_SRC) $(CRYPTO_SRC) $(STORAGE_SRC) $(UI_SRC) $(SYSTEM_SRC) $(CLI_SRC) $(SERVICES_SRC) $(COMMANDS_SRC) $(MAIN_SRC)
BIN       := $(APP)

# Test files
TEST_UNIT_SRCS := tests/core/test_config.cpp \
                  tests/crypto/test_crypto.cpp \
                  tests/cli/test_cli.cpp \
                  tests/services/test_services.cpp
TEST_SRCS := tests/test_main_gtest.cpp $(TEST_UNIT_SRCS)
TEST_BIN  := ak_tests

# Object files directory
OBJDIR    := obj

# Object files for the main application
APP_OBJS  := $(patsubst src/%.cpp,$(OBJDIR)/src/%.o,$(APP_SRCS))

# Object files for the tests
TEST_OBJS := $(OBJDIR)/tests/test_main_gtest.o $(patsubst tests/%.cpp,$(OBJDIR)/tests/%.o,$(TEST_UNIT_SRCS))
MODULE_OBJS := $(patsubst src/%.cpp,$(OBJDIR)/src/%.o,$(filter-out $(MAIN_SRC),$(APP_SRCS)))

DESTDIR   ?=
INSTALL   ?= install

PKGROOT   := pkg
DISTDIR   := dist

all: $(BIN)

test: $(TEST_BIN)
	$(MAKE) $(BIN) # Ensure the main app is built
	cd $(CURDIR) && ./$(TEST_BIN)

# Rule to create object directories
$(OBJDIR)/src/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(OBJDIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -Itests -c $< -o $@

$(BIN): $(APP_OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(TEST_BIN): $(TEST_OBJS) $(MODULE_OBJS)
	$(CXX) $(LDFLAGS) $^ -Ltests/googletest/build/lib -lgtest -lgtest_main -pthread -o $@


strip: $(BIN)
	@which strip >/dev/null 2>&1 && strip $(BIN) || true

install: $(BIN)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(APP)
	$(DESTDIR)$(BINDIR)/$(APP) install-shell
	@echo "✅ Installed $(APP) to $(DESTDIR)$(BINDIR)/$(APP)"

# Install to user's local bin directory (no sudo required)
install-user: $(BIN)
	@mkdir -p $(HOME)/.local/bin
	$(INSTALL) -m 0755 $(BIN) $(HOME)/.local/bin/$(APP)
	$(HOME)/.local/bin/$(APP) install-shell
	@echo "✅ Installed $(APP) to $(HOME)/.local/bin/$(APP)"
	@echo "💡 Make sure $(HOME)/.local/bin is in your PATH"

uninstall:
	@rm -f $(DESTDIR)$(BINDIR)/$(APP) || true
	@echo "🗑️  Uninstalled $(APP) from $(DESTDIR)$(BINDIR)/$(APP)"

uninstall-user:
	@rm -f $(HOME)/.local/bin/$(APP) || true
	@echo "🗑️  Uninstalled $(APP) from $(HOME)/.local/bin/$(APP)"

# -------------------------
# Debian package (.deb)
# -------------------------
package-deb: strip
	@which dpkg-deb >/dev/null 2>&1 || (echo "dpkg-deb not found"; exit 1)
	@rm -rf $(PKGROOT) $(DISTDIR)
	@mkdir -p $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN
	@mkdir -p $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/usr/bin
	@cp -f $(BIN) $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/usr/bin/$(APP)
	@chmod 0755 $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/usr/bin/$(APP)
	@echo "Package: $(APP)"                                              >  $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Version: $(VERSION)"                                         >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Section: utils"                                              >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Priority: optional"                                          >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Architecture: $(DEB_ARCH)"                                   >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Maintainer: You <you@example.com>"                           >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Depends: gpg, coreutils, bash, curl"                         >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Description: $(APP) — secure API key manager (CLI, C++)"     >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@mkdir -p $(DISTDIR)
	@dpkg-deb --build $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH) $(DISTDIR)/$(APP)_$(VERSION)_$(DEB_ARCH).deb
	@echo "🎁 Built $(DISTDIR)/$(APP)_$(VERSION)_$(DEB_ARCH).deb"

# -------------------------
# RPM package (.rpm)
# Tries native rpmbuild; falls back to fpm if available.
# -------------------------
package-rpm: strip
	@if which rpmbuild >/dev/null 2>&1; then \
	  echo "▶ using rpmbuild"; \
	  rm -rf $(PKGROOT)/rpm $(DISTDIR); \
	  mkdir -p $(PKGROOT)/rpm/{BUILD,RPMS,SOURCES,SPECS,SRPMS,BUILDROOT}; \
	  mkdir -p $(PKGROOT)/rpm/BUILDROOT/$(APP)-$(VERSION)-1.$(RPM_ARCH)/usr/bin; \
	  cp -f $(BIN) $(PKGROOT)/rpm/BUILDROOT/$(APP)-$(VERSION)-1.$(RPM_ARCH)/usr/bin/$(APP); \
	  chmod 0755 $(PKGROOT)/rpm/BUILDROOT/$(APP)-$(VERSION)-1.$(RPM_ARCH)/usr/bin/$(APP); \
	  echo "Name:           $(APP)"                                   >  $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "Version:        $(VERSION)"                                >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "Release:        1%{?dist}"                                 >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "Summary:        $(APP) — secure API key manager (CLI)"     >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "License:        MIT"                                       >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "URL:            https://example.com/$(APP)"                >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "BuildArch:      $(RPM_ARCH)"                               >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "Requires:       gpg, coreutils, bash, curl"                >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%description"                                             >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "$(APP) — secure API key manager (C++ CLI)."               >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%install"                                                 >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "mkdir -p %{buildroot}/usr/bin"                            >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "install -m 0755 $(APP) %{buildroot}/usr/bin/$(APP)"       >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%files"                                                   >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%attr(0755,root,root) /usr/bin/$(APP)"                    >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%prep"                                                    >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%build"                                                   >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%clean"                                                   >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  cp -f $(BIN) $(PKGROOT)/rpm/; \
	  rpmbuild --define "_topdir $(abspath $(PKGROOT))/rpm" \
	           --define "_builddir %{_topdir}/BUILD" \
	           --define "_rpmdir %{_topdir}/RPMS" \
	           --define "_srcrpmdir %{_topdir}/SRPMS" \
	           --define "_specdir %{_topdir}/SPECS" \
	           --define "_sourcedir %{_topdir}/" \
	           -bb $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  mkdir -p $(DISTDIR); \
	  cp -f $(PKGROOT)/rpm/RPMS/$(RPM_ARCH)/*.rpm $(DISTDIR)/; \
	  echo "🎁 Built RPM(s) in $(DISTDIR)"; \
	elif which fpm >/dev/null 2>&1; then \
	  echo "▶ using fpm (fallback)"; \
	  rm -rf $(DISTDIR) && mkdir -p $(DISTDIR); \
	  fpm -s dir -t rpm -n $(APP) -v $(VERSION) \
	      --architecture $(RPM_ARCH) \
	      --description "$(APP) — secure API key manager (CLI)" \
	      --depends gpg --depends coreutils --depends bash --depends curl \
	      $(BIN)=/usr/bin/$(APP) \
	      -p $(DISTDIR)/; \
	  echo "🎁 Built RPM in $(DISTDIR)"; \
	else \
	  echo "Neither rpmbuild nor fpm found. Install one of them."; \
	  exit 1; \
	fi

dist: package-deb package-rpm

clean:
	@rm -f $(BIN) $(TEST_BIN)
	@rm -rf $(PKGROOT) $(DISTDIR) $(OBJDIR)
	@echo "🧹 Cleaned"
