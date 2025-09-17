# Makefile for ak (C++ API key manager)
# Build, install, and package (.deb / .rpm)
#
# Usage:
#   make                # build ./ak
#   sudo make install   # install to /usr/local/bin/ak
#   sudo make uninstall # remove installed binary
#   make test           # run unit tests
#   make package-deb    # builds ./dist/ak_<ver>_<arch>.deb (needs dpkg-deb)
#   make package-rpm    # builds ./dist/ak-<ver>-1.<arch>.rpm (needs rpmbuild or fpm)
#   make publish-ppa    # build signed source and upload to Launchpad PPA
#   make release        # minor version bump + test + commit + tag + publish everywhere
#   make release-patch  # patch version bump + test + commit + tag + publish everywhere
#   make release-major  # major version bump + test + commit + tag + publish everywhere
#   V_BUMP=patch make release     # override default (minor) with patch
#   make clean

APP       ?= ak
VERSION   ?= 4.2.27
V_BUMP   ?= minor
# Detect arch name for packages
UNAME_M   := $(shell uname -m)
# Map to Debian/RPM arch tags
DEB_ARCH  := $(if $(filter $(UNAME_M),x86_64 amd64),amd64,$(UNAME_M))
RPM_ARCH  := $(if $(filter $(UNAME_M),x86_64 amd64),x86_64,$(UNAME_M))

CXX       ?= g++
CXXFLAGS  ?= -std=c++17 -O2 -pipe -Wall -Wextra -Iinclude -Itests/googletest/googletest/include -Itests/googletest/googlemock/include
CXXFLAGS_COV := -std=c++17 -O0 -g -pipe -Wall -Wextra -Iinclude -Itests/googletest/googletest/include -Itests/googletest/googlemock/include --coverage
LDFLAGS   ?=
LDFLAGS_COV := --coverage
PREFIX    ?= /usr/local
BINDIR    ?= $(PREFIX)/bin

# Launchpad PPA settings
SERIES    ?= $(shell dpkg-parsechangelog -S Distribution 2>/dev/null || echo noble)
PPA       ?= ppa:apertacodex/ak
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

.PHONY: all test strip install install-user uninstall uninstall-user \
        coverage coverage-html coverage-summary clean-coverage clean-obj \
        package-deb package-rpm dist publish publish-patch publish-minor publish-major \
        bump-patch bump-minor bump-major build-release commit-and-push commit-repository-files publish-ppa \
        release release-patch release-minor release-major publish-all clean

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

# Test coverage targets
coverage: clean-coverage
	@echo "🔍 Building with coverage instrumentation..."
	$(MAKE) clean-obj
	$(MAKE) CXXFLAGS="$(CXXFLAGS) $(CXXFLAGS_COV)" LDFLAGS="$(LDFLAGS_COV)" $(TEST_BIN)
	@echo "🧪 Running tests with coverage..."
	cd $(CURDIR) && ./$(TEST_BIN)
	@echo "📊 Generating coverage report..."
	@if which lcov >/dev/null 2>&1; then \
		lcov --capture --directory obj --output-file coverage.info --ignore-errors path,inconsistent --quiet; \
		echo "✅ Coverage analysis complete!"; \
		echo "📈 Coverage data saved to coverage.info"; \
	else \
		echo "⚠️  lcov not found. Install lcov for coverage reports: sudo apt-get install lcov"; \
	fi

coverage-html: coverage
	@echo "🌐 Generating HTML coverage report..."
	@if which lcov >/dev/null 2>&1; then \
		lcov --remove coverage.info '/usr/*' '*/tests/googletest/*' --output-file coverage.info --quiet; \
		genhtml coverage.info --output-directory coverage_html --quiet; \
		echo "📊 HTML coverage report generated in coverage_html/index.html"; \
	else \
		echo "⚠️  lcov not found. Install lcov for HTML reports: sudo apt-get install lcov"; \
	fi

coverage-summary: coverage
	@echo "📊 Coverage Summary:"
	@echo "===================="
	@if [ -f coverage.info ]; then \
		lcov --summary coverage.info | grep -E "(lines|functions)" | sed 's/^/📈 /'; \
	else \
		echo "⚠️  No coverage data available. Run 'make coverage' first."; \
	fi
	@echo "===================="

clean-coverage:
	@rm -f *.gcov *.gcda *.gcno coverage.info
	@rm -rf coverage_html
	@find $(OBJDIR) \( -name "*.gcda" -o -name "*.gcno" \) -type f -exec rm -f {} \; 2>/dev/null || true

# Clean only object files (for rebuilding with coverage)
clean-obj:
	@rm -rf $(OBJDIR)
	@mkdir -p $(OBJDIR)/src/core $(OBJDIR)/src/crypto $(OBJDIR)/src/cli $(OBJDIR)/src/commands $(OBJDIR)/src/services $(OBJDIR)/src/storage $(OBJDIR)/src/ui $(OBJDIR)/src/system $(OBJDIR)/tests/core $(OBJDIR)/tests/crypto $(OBJDIR)/tests/cli $(OBJDIR)/tests/services

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
	@echo "Version: $(VERSION)"                                          >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Section: utils"                                              >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Priority: optional"                                          >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Architecture: $(DEB_ARCH)"                                   >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
	@echo "Maintainer: Moussa Mokhtari <me@moussamokhtari.com>"         >> $(PKGROOT)/deb/$(APP)_$(VERSION)_$(DEB_ARCH)/DEBIAN/control
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
	  mkdir -p $(PKGROOT)/rpm/BUILD $(PKGROOT)/rpm/RPMS $(PKGROOT)/rpm/SOURCES $(PKGROOT)/rpm/SPECS $(PKGROOT)/rpm/SRPMS $(PKGROOT)/rpm/BUILDROOT; \
	  mkdir -p $(PKGROOT)/rpm/BUILDROOT/$(APP)-$(VERSION)-1.$(RPM_ARCH)/usr/bin; \
	  cp -f $(BIN) $(PKGROOT)/rpm/BUILDROOT/$(APP)-$(VERSION)-1.$(RPM_ARCH)/usr/bin/$(APP); \
	  chmod 0755 $(PKGROOT)/rpm/BUILDROOT/$(APP)-$(VERSION)-1.$(RPM_ARCH)/usr/bin/$(APP); \
	  echo "Name:           $(APP)"                                   >  $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "Version:        $(VERSION)"                                >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "Release:        1%{?dist}"                                 >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "Summary:        $(APP) — secure API key manager (CLI)"     >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "License:        MIT"                                       >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "URL:            https://apertacodex.github.io/$(APP)"                >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "BuildArch:      $(RPM_ARCH)"                               >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "Requires:       gpg, coreutils, bash, curl"                >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%description"                                             >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "$(APP) — secure API key manager (C++ CLI)."               >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "%install"                                                 >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "mkdir -p %{buildroot}/usr/bin"                            >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
	  echo "install -m 0755 %{_sourcedir}/$(APP) %{buildroot}/usr/bin/$(APP)" >> $(PKGROOT)/rpm/SPECS/$(APP).spec; \
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

# -------------------------
# Publishing and versioning
# -------------------------
publish: publish-patch

publish-apt:
	@echo "📦 Updating APT repository..."
	@cmake -DPROJECT_DIR=$(CURDIR) -P cmake/apt_publish.cmake

# -------------------------
# Launchpad PPA (multi-distribution)
# -------------------------
publish-macos:
	@echo "🍎 Building macOS packages (Linux-compatible)..."
	@if command -v hdiutil >/dev/null 2>&1; then \
		echo "📦 macOS detected - building native packages..."; \
		cd macos/scripts && ./package-all.sh; \
		echo "✅ Native macOS packages built in build/macos-packages/"; \
	elif command -v 7z >/dev/null 2>&1; then \
		echo "📦 Linux detected - building cross-platform packages..."; \
		./macos/scripts/create-dmg-linux.sh; \
		echo "✅ Cross-platform macOS packages built in ak-macos-repo/packages/"; \
	else \
		echo "⚠️  Neither macOS nor 7z available - skipping macOS package build"; \
	fi

publish-ppa:
	@echo "🚀 Publishing to Launchpad PPA..."
	@chmod +x ./ppa-upload.sh || true
	@sudo make clean || true
	@sudo rm -rf obj-x86_64-linux-gnu/ build/ ak ak_tests obj/ pkg/ dist/ || true
	@sudo chown -R $(USER):$(USER) debian/ .git/ || true
	@PPA="$(PPA)" ./ppa-upload.sh

publish-ppa-single:
	@echo "🚀 Publishing to Launchpad PPA $(PPA) (series=$(SERIES))..."
	@chmod +x ./publish-ppa.sh || true
	@PPA="$(PPA)" SERIES="$(SERIES)" ./publish-ppa.sh

publish-patch:
	@$(MAKE) bump-patch build-release commit-and-push publish-apt

publish-minor:
	@$(MAKE) bump-minor build-release commit-and-push publish-apt

publish-major:
	@$(MAKE) bump-major build-release commit-and-push publish-apt

# -------------------------
# Comprehensive Release Commands (Default: minor)
# Usage:
#   make release              # minor version bump + publish everywhere
#   make release-patch        # patch version bump + publish everywhere
#   make release-minor        # minor version bump + publish everywhere
#   make release-major        # major version bump + publish everywhere
#   V_BUMP=patch make release    # override default with flag
#   V_BUMP=major make release    # override default with flag
# -------------------------

release:
	@echo "🚀 Starting comprehensive release ($(V_BUMP) version bump)..."
	@case "$(V_BUMP)" in \
		patch) $(MAKE) release-patch ;; \
		minor) $(MAKE) release-minor ;; \
		major) $(MAKE) release-major ;; \
		*) echo "❌ Invalid V_BUMP: $(V_BUMP). Use: patch, minor, or major"; exit 1 ;; \
	esac

release-patch:
	@echo "🚀 Release: patch version bump + publish to all repositories..."
	@$(MAKE) bump-patch build-release commit-and-push publish-all commit-repository-files

release-minor:
	@echo "🚀 Release: minor version bump + publish to all repositories..."
	@$(MAKE) bump-minor build-release commit-and-push publish-all commit-repository-files

release-major:
	@echo "🚀 Release: major version bump + publish to all repositories..."
	@$(MAKE) bump-major build-release commit-and-push publish-all commit-repository-files

publish-all:
	@echo "📦 Publishing to all repositories..."
	@echo "📦 1/3 Publishing to Launchpad PPA (needs clean working tree)..."
	@$(MAKE) publish-ppa || (echo "⚠️  PPA publish failed - continuing with other targets"; true)
	@echo "📦 2/3 Publishing to APT repository (GitHub Pages)..."
	@$(MAKE) publish-apt
	@echo "📦 3/3 Building macOS DMG packages..."
	@$(MAKE) publish-macos || (echo "⚠️  macOS DMG build failed - continuing with other targets"; true)
	@echo "✅ Published to all repositories"
	@echo "📦 Distribution links:"
	@echo "   - Launchpad PPA: https://launchpad.net/~apertacodex/+archive/ubuntu/ak"
	@echo "   - APT Repository: https://apertacodex.github.io/ak/ak-apt-repo"
	@echo "   - macOS Repository: https://apertacodex.github.io/ak/ak-macos-repo"
	@echo "   - GitHub Release: https://github.com/ApertaCodex/ak/releases/tag/v$$(grep "^VERSION" Makefile | head -1 | cut -d'=' -f2 | tr -d ' ?')"

bump-patch:
	@echo "🔄 Bumping patch version..."
	@current_version=$$(grep "^VERSION" Makefile | head -1 | cut -d'=' -f2 | tr -d ' ?'); \
	major=$$(echo $$current_version | cut -d'.' -f1); \
	minor=$$(echo $$current_version | cut -d'.' -f2); \
	patch=$$(echo $$current_version | cut -d'.' -f3); \
	new_patch=$$(($$patch + 1)); \
	new_version="$$major.$$minor.$$new_patch"; \
	echo "📈 Version: $$current_version → $$new_version"; \
	sed -i "s/^VERSION.*=.*/VERSION   ?= $$new_version/" Makefile; \
	sed -i "s/const std::string AK_VERSION = \".*\";/const std::string AK_VERSION = \"$$new_version\";/" src/core/config.cpp; \
	sed -i "s/#define AK_VERSION_STRING \".*\"/#define AK_VERSION_STRING \"$$new_version\"/" src/core/config.cpp; \
	sed -i "s/set(AK_VERSION \".*\"/set(AK_VERSION \"$$new_version\"/" CMakeLists.txt; \
	sed -i "s/^ak (.*)/ak ($$new_version)/" debian/changelog; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+_amd64\.deb/ak_$$new_version\_amd64.deb/g" index.html; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+\(-[0-9]\+\)\?_amd64\.deb/ak_$$new_version\_amd64.deb/g" ak-apt-repo/index.html; \
	sed -i "s/Latest version:[^<]*/Latest version: $$new_version/g" ak-apt-repo/index.html; \
	sed -i "s/Latest version: [0-9]\+\.[0-9]\+\.[0-9]\+/Latest version: $$new_version/g" ak-macos-repo/index.html; \
	sed -i "s/AK-[0-9]\+\.[0-9]\+\.[0-9]\+/AK-$$new_version/g" ak-macos-repo/index.html; \
	sed -i "s/- \*\*Version\*\*: [0-9]\+\.[0-9]\+\.[0-9]\+/- **Version**: $$new_version/" README.md; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+_amd64\.deb/ak_$$new_version\_amd64.deb/g" README.md; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+_amd64\.deb/ak_$$new_version\_amd64.deb/g" README.md; \
	sed -i "s/version: \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version: \"$$new_version\"/" CITATION.cff; \
	sed -i "s/\"version\": \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/\"version\": \"$$new_version\"/" codemeta.json; \
	sed -i "s/- \*\*Version\*\*: \`\?[0-9]\+\.[0-9]\+\.[0-9]\+\(-[0-9]\+\)\?\`\?/- **Version**: \`$$new_version\`/" DEBIAN_PUBLISHING.md; \
	sed -i "s/AK version [0-9]\+\.[0-9]\+\.[0-9]\+/AK version $$new_version/" docs/MANUAL.md; \
	sed -i "s/\"AK [0-9]\+\.[0-9]\+\.[0-9]\+\"/\"AK $$new_version\"/" docs/ak.1; \
	sed -i "s/AK version [0-9]\+\.[0-9]\+\.[0-9]\+/AK version $$new_version/" docs/ak.1; \
	sed -i "s/version \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version \"$$new_version\"/" Formula/ak.rb; \
	sed -i "s|archive/v[0-9]\+\.[0-9]\+\.[0-9]\+\.tar\.gz|archive/v$$new_version.tar.gz|" Formula/ak.rb; \
	sed -i "s/version \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version \"$$new_version\"/" macos/homebrew/generated/ak.rb; \
	sed -i "s|archive/v[0-9]\+\.[0-9]\+\.[0-9]\+\.tar\.gz|archive/v$$new_version.tar.gz|" macos/homebrew/generated/ak.rb; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-app-bundle.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-dmg.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-pkg.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/package-all.sh; \
	echo "✅ Updated version to $$new_version in ALL files (code, docs, HTML, metadata, macOS)"

bump-minor:
	@echo "🔄 Bumping minor version..."
	@current_version=$$(grep "^VERSION" Makefile | head -1 | cut -d'=' -f2 | tr -d ' ?'); \
	major=$$(echo $$current_version | cut -d'.' -f1); \
	minor=$$(echo $$current_version | cut -d'.' -f2); \
	new_minor=$$(($$minor + 1)); \
	new_version="$$major.$$new_minor.0"; \
	echo "📈 Version: $$current_version → $$new_version"; \
	sed -i "s/^VERSION.*=.*/VERSION   ?= $$new_version/" Makefile; \
	sed -i "s/const std::string AK_VERSION = \".*\";/const std::string AK_VERSION = \"$$new_version\";/" src/core/config.cpp; \
	sed -i "s/#define AK_VERSION_STRING \".*\"/#define AK_VERSION_STRING \"$$new_version\"/" src/core/config.cpp; \
	sed -i "s/set(AK_VERSION \".*\"/set(AK_VERSION \"$$new_version\"/" CMakeLists.txt; \
	sed -i "s/^ak (.*)/ak ($$new_version)/" debian/changelog; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+_amd64\.deb/ak_$$new_version\_amd64.deb/g" index.html; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+\(-[0-9]\+\)\?_amd64\.deb/ak_$$new_version\_amd64.deb/g" ak-apt-repo/index.html; \
	sed -i "s/Latest version:[^<]*/Latest version: $$new_version/g" ak-apt-repo/index.html; \
	sed -i "s/- \*\*Version\*\*: [0-9]\+\.[0-9]\+\.[0-9]\+/- **Version**: $$new_version/" README.md; \
	sed -i "s/version: \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version: \"$$new_version\"/" CITATION.cff; \
	sed -i "s/\"version\": \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/\"version\": \"$$new_version\"/" codemeta.json; \
	sed -i "s/- \*\*Version\*\*: \`\?[0-9]\+\.[0-9]\+\.[0-9]\+\(-[0-9]\+\)\?\`\?/- **Version**: \`$$new_version\`/" DEBIAN_PUBLISHING.md; \
	sed -i "s/AK version [0-9]\+\.[0-9]\+\.[0-9]\+/AK version $$new_version/" docs/MANUAL.md; \
	sed -i "s/\"AK [0-9]\+\.[0-9]\+\.[0-9]\+\"/\"AK $$new_version\"/" docs/ak.1; \
	sed -i "s/AK version [0-9]\+\.[0-9]\+\.[0-9]\+/AK version $$new_version/" docs/ak.1; \
	sed -i "s/version \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version \"$$new_version\"/" Formula/ak.rb; \
	sed -i "s|archive/v[0-9]\+\.[0-9]\+\.[0-9]\+\.tar\.gz|archive/v$$new_version.tar.gz|" Formula/ak.rb; \
	sed -i "s/version \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version \"$$new_version\"/" macos/homebrew/generated/ak.rb; \
	sed -i "s|archive/v[0-9]\+\.[0-9]\+\.[0-9]\+\.tar\.gz|archive/v$$new_version.tar.gz|" macos/homebrew/generated/ak.rb; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-app-bundle.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-dmg.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-pkg.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/package-all.sh; \
	echo "✅ Updated version to $$new_version in ALL files (code, docs, HTML, metadata, macOS)"

bump-major:
	@echo "🔄 Bumping major version..."
	@current_version=$$(grep "^VERSION" Makefile | head -1 | cut -d'=' -f2 | tr -d ' ?'); \
	major=$$(echo $$current_version | cut -d'.' -f1); \
	new_major=$$(($$major + 1)); \
	new_version="$$new_major.0.0"; \
	echo "📈 Version: $$current_version → $$new_version"; \
	sed -i "s/^VERSION.*=.*/VERSION   ?= $$new_version/" Makefile; \
	sed -i "s/const std::string AK_VERSION = \".*\";/const std::string AK_VERSION = \"$$new_version\";/" src/core/config.cpp; \
	sed -i "s/#define AK_VERSION_STRING \".*\"/#define AK_VERSION_STRING \"$$new_version\"/" src/core/config.cpp; \
	sed -i "s/set(AK_VERSION \".*\"/set(AK_VERSION \"$$new_version\"/" CMakeLists.txt; \
	sed -i "s/^ak (.*)/ak ($$new_version)/" debian/changelog; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+_amd64\.deb/ak_$$new_version\_amd64.deb/g" index.html; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+\(-[0-9]\+\)\?_amd64\.deb/ak_$$new_version\_amd64.deb/g" ak-apt-repo/index.html; \
	sed -i "s/Latest version:[^<]*/Latest version: $$new_version/g" ak-apt-repo/index.html; \
	sed -i "s/- \*\*Version\*\*: [0-9]\+\.[0-9]\+\.[0-9]\+/- **Version**: $$new_version/" README.md; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+_amd64\.deb/ak_$$new_version\_amd64.deb/g" README.md; \
	sed -i "s/ak_[0-9]\+\.[0-9]\+\.[0-9]\+_amd64\.deb/ak_$$new_version\_amd64.deb/g" README.md; \
	sed -i "s/version: \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version: \"$$new_version\"/" CITATION.cff; \
	sed -i "s/\"version\": \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/\"version\": \"$$new_version\"/" codemeta.json; \
	sed -i "s/- \*\*Version\*\*: \`\?[0-9]\+\.[0-9]\+\.[0-9]\+\(-[0-9]\+\)\?\`\?/- **Version**: \`$$new_version\`/" DEBIAN_PUBLISHING.md; \
	sed -i "s/AK version [0-9]\+\.[0-9]\+\.[0-9]\+/AK version $$new_version/" docs/MANUAL.md; \
	sed -i "s/\"AK [0-9]\+\.[0-9]\+\.[0-9]\+\"/\"AK $$new_version\"/" docs/ak.1; \
	sed -i "s/AK version [0-9]\+\.[0-9]\+\.[0-9]\+/AK version $$new_version/" docs/ak.1; \
	sed -i "s/version \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version \"$$new_version\"/" Formula/ak.rb; \
	sed -i "s|archive/v[0-9]\+\.[0-9]\+\.[0-9]\+\.tar\.gz|archive/v$$new_version.tar.gz|" Formula/ak.rb; \
	sed -i "s/version \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/version \"$$new_version\"/" macos/homebrew/generated/ak.rb; \
	sed -i "s|archive/v[0-9]\+\.[0-9]\+\.[0-9]\+\.tar\.gz|archive/v$$new_version.tar.gz|" macos/homebrew/generated/ak.rb; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-app-bundle.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-dmg.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/create-pkg.sh; \
	sed -i "s/VERSION=\"\$${AK_VERSION:-[0-9]\+\.[0-9]\+\.[0-9]\+}\"/VERSION=\"\$${AK_VERSION:-$$new_version}\"/" macos/scripts/package-all.sh; \
	echo "✅ Updated version to $$new_version in ALL files (code, docs, HTML, metadata, macOS)"

build-release: clean
	@echo "🏗️  Building complete production release..."
	@echo "📋 Step 1/4: Building optimized binary..."
	@$(MAKE) all
	@$(MAKE) strip
	@echo "📦 Step 2/4: Building Debian package..."
	@$(MAKE) package-deb
	@echo "📦 Step 3/4: Building RPM package (if available)..."
	@$(MAKE) package-rpm || (echo "⚠️  RPM build skipped (tools not available)"; true)
	@echo "📦 Step 4/4: Building distribution packages..."
	@$(MAKE) dist || (echo "⚠️  Some distribution packages failed"; true)
	@echo "✅ Complete production release build finished"
	@echo "📁 Artifacts created:"
	@echo "   - Binary: ./ak (stripped, optimized)"
	@echo "   - Packages: ./dist/"
	@ls -la ./dist/ 2>/dev/null || echo "   (No dist directory - packages may be in parent directory)"


commit-and-push:
	@echo "📦 Committing and pushing release..."
	@new_version=$$(grep "^VERSION" Makefile | head -1 | cut -d'=' -f2 | tr -d ' ?'); \
	if ! git diff --quiet; then \
		git add .gitignore Makefile CMakeLists.txt src/core/config.cpp debian/changelog debian/source/exclude \
		        index.html ak-apt-repo/index.html ak-macos-repo/index.html \
		        README.md CITATION.cff codemeta.json DEBIAN_PUBLISHING.md \
		        docs/MANUAL.md docs/ak.1 install-ak.sh Formula/ak.rb release.sh \
		        macos/homebrew/generated/ak.rb macos/scripts/*.sh; \
		git rm --cached ak debian/files pkg/ dist/ ak_tests 2>/dev/null || true; \
		git reset HEAD ak-apt-repo/pool/main/*.deb ak-apt-repo/dists/*/InRelease ak-apt-repo/dists/*/Release ak-apt-repo/dists/*/Release.gpg \
		            ak-apt-repo/dists/*/main/binary-amd64/Packages* ak-macos-repo/packages/*.tar.xz \
		            ak-macos-repo/packages/*.7z ak-macos-repo/packages/*.md 2>/dev/null || true; \
		git commit -m "🚀 Release v$$new_version"; \
		git tag "v$$new_version"; \
		echo "🏷️  Created tag v$$new_version"; \
		if git remote get-url origin >/dev/null 2>&1; then \
			echo "📤 Pushing to main branch..."; \
			git push origin main; \
			git push origin "v$$new_version"; \
			echo "✅ Successfully published v$$new_version to GitHub"; \
		else \
			echo "⚠️  No remote 'origin' found. Skipping push to GitHub."; \
			echo "✅ Version bumped and committed locally as v$$new_version"; \
		fi; \
	else \
		echo "⚠️  No changes to commit"; \
	fi

commit-repository-files:
	@echo "📦 Committing repository files..."
	@new_version=$$(grep "^VERSION" Makefile | head -1 | cut -d'=' -f2 | tr -d ' ?'); \
	if ! git diff --quiet; then \
		git add ak-apt-repo/ ak-macos-repo/ 2>/dev/null || true; \
		if ! git diff --cached --quiet 2>/dev/null; then \
			git commit -m "📦 Update repositories for v$$new_version"; \
			if git remote get-url origin >/dev/null 2>&1; then \
				echo "📤 Pushing repository updates..."; \
				git push origin main; \
			fi; \
			echo "✅ Repository files committed and pushed"; \
		else \
			echo "📦 No repository changes to commit"; \
		fi; \
	else \
		echo "📦 No repository changes to commit"; \
	fi

clean: clean-coverage
	@rm -f $(BIN) $(TEST_BIN)
	@rm -rf $(PKGROOT) $(DISTDIR) $(OBJDIR) build
	@echo "🧹 Cleaned"
