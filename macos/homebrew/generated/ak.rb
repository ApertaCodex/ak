# typed: false
# frozen_string_literal: true

# AK Homebrew Formula
class Ak < Formula
  desc "Advanced Key manager and cryptographic utility for secure storage and operations"
  homepage "https://github.com/apertacodex/ak"
  url "https://github.com/apertacodex/ak/archive/v4.8.0.tar.gz"
  version "4.8.0"
  sha256 "25727efbc2c0bbec8643c14ae7094116bb3ed5e6a404580dfa236213e9f88b49"
  license "MIT"


  # Build dependencies (alphabetical order)
  depends_on "cmake" => :build
  depends_on "pkg-config" => :build

  # System dependencies
  depends_on macos: :monterey

  # Runtime dependencies (alphabetical order)
  depends_on "openssl@3"
  depends_on "qt@6"

  def install
    # Set Qt6 environment variables
    ENV["Qt6_DIR"] = Formula["qt@6"].opt_lib/"cmake/Qt6"
    ENV["QT_DIR"] = Formula["qt@6"].opt_prefix

    # Create build directory
    mkdir "build" do
      # Configure with CMake
      system "cmake", "..",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_INSTALL_PREFIX=#{prefix}",
             "-DBUILD_GUI=ON",
             "-DBUILD_TESTS=OFF",
             "-DBUILD_TESTING=OFF",
             *std_cmake_args

      # Build the project
      system "make", "-j#{ENV.make_jobs}"

      # Install the project
      system "make", "install"
    end

    # Install shell completions
    install_completions
  end

  def install_completions
    # Generate completions using the installed binary
    # AK supports: ak completion bash/zsh/fish
    output = Utils.safe_popen_read("#{bin}/ak", "completion", "bash")
    (bash_completion/"ak").write output

    output = Utils.safe_popen_read("#{bin}/ak", "completion", "zsh")
    (zsh_completion/"_ak").write output

    output = Utils.safe_popen_read("#{bin}/ak", "completion", "fish")
    (fish_completion/"ak.fish").write output
  end

  def caveats
    <<~EOS
      AK has been installed with both CLI and GUI components.

      CLI Usage:
        ak --help
        ak version

      Usage:
        ak --help          # Show all commands and options
        ak gui             # Launch GUI interface (if available)

      For shell completion, add the following to your shell profile:

      Bash:
        echo 'source $(brew --prefix)/etc/bash_completion.d/ak' >> ~/.bashrc

      Zsh:
        echo 'fpath=($(brew --prefix)/share/zsh/site-functions $fpath)' >> ~/.zshrc
        echo 'autoload -Uz compinit && compinit' >> ~/.zshrc

      Fish:
        echo 'set -gx fish_complete_path (brew --prefix)/share/fish/vendor_completions.d $fish_complete_path' >> ~/.config/fish/config.fish
    EOS
  end

  test do
    # Test CLI functionality
    assert_match version.to_s, shell_output("#{bin}/ak --version")

    # Test basic CLI operations (non-interactive)
    assert_match "AK", shell_output("#{bin}/ak --help")

    # Test that GUI binary exists and is executable
    assert_path_exists bin/"ak-gui"
    assert_predicate bin/"ak-gui", :executable?

    # Test configuration directory creation
    system "#{bin}/ak", "config", "init", "--non-interactive"
    assert_predicate testpath/".ak", :directory?
  end
end