#!/usr/bin/env bash
# install-ak.sh — install a standalone "ak" CLI
set -euo pipefail

PREFIX="${1:-$HOME/.local}"   # allow: ./install-ak.sh /custom/prefix
[[ "${1:-}" == "--prefix" && -n "${2:-}" ]] && { PREFIX="$2"; shift 2; } || true
BIN_DIR="${PREFIX%/}/bin"
AK_PATH="${BIN_DIR}/ak"

mkdir -p "$BIN_DIR"

cat >"$AK_PATH" <<"AKCLI"
#!/usr/bin/env bash
# ak — Secure API Key Manager (standalone bash CLI)
# Features:
# - Backends: pass | secret-tool | gpg (auto-detect)
# - Profiles: save/load/list (names only; values stay in secure storage)
# - Import/Export: env | dotenv | json | yaml
# - Run scoped env: ak run --profile p -- <cmd>
# - Tests (HTTP pings): --all or single service
# - Guard: simple pre-commit secret scan (gitleaks if present; grep fallback)

set -euo pipefail

# --------- Globals ----------
AK_BACKEND=""
AK_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/ak"
AK_VAULT="$AK_DIR/keys.env.gpg"
AK_PROFILES_DIR="$AK_DIR/profiles"
AK_CLIP_TTL="${AK_CLIP_TTL:-45}"
AK_JSON=0
AK_MASK_PREFIX=8
AK_MASK_SUFFIX=4
AWK_BIN="$(command -v gawk || command -v awk || echo awk)"
SORT_BIN="$(command -v sort || echo sort)"

declare -A AK_SERVICE_KEYS=(
  [anthropic]=ANTHROPIC_API_KEY
  [azure_openai]=AZURE_OPENAI_API_KEY
  [brave]=BRAVE_API_KEY
  [cohere]=COHERE_API_KEY
  [deepseek]=DEEPSEEK_API_KEY
  [exa]=EXA_API_KEY
  [fireworks]=FIREWORKS_API_KEY
  [gemini]=GEMINI_API_KEY
  [groq]=GROQ_API_KEY
  [huggingface]=HUGGINGFACE_TOKEN
  [mistral]=MISTRAL_API_KEY
  [openai]=OPENAI_API_KEY
  [openrouter]=OPENROUTER_API_KEY
  [perplexity]=PERPLEXITY_API_KEY
  [sambanova]=SAMBANOVA_API_KEY
  [tavily]=TAVILY_API_KEY
  [together]=TOGETHER_API_KEY
  [xai]=XAI_API_KEY
)

# Reverse mapping: canonical key -> service name
declare -A AK_KEY_TO_SERVICE=(
  [ANTHROPIC_API_KEY]=anthropic
  [AZURE_OPENAI_API_KEY]=azure_openai
  [BRAVE_API_KEY]=brave
  [COHERE_API_KEY]=cohere
  [DEEPSEEK_API_KEY]=deepseek
  [EXA_API_KEY]=exa
  [FIREWORKS_API_KEY]=fireworks
  [GEMINI_API_KEY]=gemini
  [GROQ_API_KEY]=groq
  [HUGGINGFACE_TOKEN]=huggingface
  [MISTRAL_API_KEY]=mistral
  [OPENAI_API_KEY]=openai
  [OPENROUTER_API_KEY]=openrouter
  [PERPLEXITY_API_KEY]=perplexity
  [SAMBANOVA_API_KEY]=sambanova
  [TAVILY_API_KEY]=tavily
  [TOGETHER_API_KEY]=together
  [XAI_API_KEY]=xai
)

# Additional known key variations for smart import mapping
declare -A AK_KEY_ALIASES=(
  [GOOGLE_GENAI_API_KEY]=GEMINI_API_KEY
  [GOOGLE_API_KEY]=GEMINI_API_KEY
  [HUGGINGFACE_API_KEY]=HUGGINGFACE_TOKEN
  [HF_TOKEN]=HUGGINGFACE_TOKEN
  [HF_API_KEY]=HUGGINGFACE_TOKEN
  [ANTHROPIC_TOKEN]=ANTHROPIC_API_KEY
  [CLAUDE_API_KEY]=ANTHROPIC_API_KEY
  [GPT_API_KEY]=OPENAI_API_KEY
  [OPENAI_TOKEN]=OPENAI_API_KEY
)

# --------- Utils ----------
ak_has() { command -v "$1" >/dev/null 2>&1; }
ak_mkdirs() { mkdir -p "$AK_DIR" "$AK_PROFILES_DIR" 2>/dev/null || true; }
ak_warn() { [[ "$AK_JSON" = 1 ]] || printf '⚠️  %s\n' "$*" >&2; }
ak_err()  { if [[ "$AK_JSON" = 1 ]]; then printf '{"ok":false,"error":"%s"}\n' "${*//\"/\\\"}"; else printf '❌ %s\n' "$*" >&2; fi; exit 1; }
ak_ok()   { [[ "$AK_JSON" = 1 ]] || printf '✅ %s\n' "$*" >&2; }
ak_mask() {
  local v="${1:-}"; [[ -z "$v" ]] && { printf "(empty)\n"; return; }
  local pre=${v:0:$AK_MASK_PREFIX}; local suf=${v: -$AK_MASK_SUFFIX}
  printf "%s***%s\n" "$pre" "$suf"
}
ak_shell_quote() {
  local v="$1"
  if printf '%s' "$v" | grep -qE '[[:space:]"'\''\\$#]|\n|\r'; then
    v="$(printf '%s' "$v" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/\$/\\$/g')"
    printf '"%s"\n' "$v"
  else
    printf '%s\n' "$v"
  fi
}
ak_json_escape() {
  printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/\t/\\t/g' -e 's/\r/\\r/g' -e 's/\n/\\n/g'
}
ak_sed_inplace() { # portable sed -i
  if sed --version >/dev/null 2>&1; then sed -i "$@"; else sed -i '' "$@" 2>/dev/null || sed -i "$@"; fi
}
ak_normalize_key() {
  local input_key="$1"
  # Check if it's a known alias that should be mapped to canonical form
  if [[ -n "${AK_KEY_ALIASES[$input_key]:-}" ]]; then
    printf '%s' "${AK_KEY_ALIASES[$input_key]}"
  else
    printf '%s' "$input_key"
  fi
}

# --------- Backend detection & storage ----------
ak_detect_backend() {
  [[ -n "$AK_BACKEND" ]] && return 0
  if ak_has pass; then AK_BACKEND="pass"
  elif ak_has secret-tool; then AK_BACKEND="secret-tool"
  else AK_BACKEND="gpg"
  fi
}
ak_store_set() {
  ak_detect_backend; ak_mkdirs
  local k="$1" v="$2"
  case "$AK_BACKEND" in
    pass) printf '%s' "$v" | pass insert -m "ak/$k" >/dev/null ;;
    secret-tool) printf '%s' "$v" | secret-tool store --label="ak:$k" ak key "$k" ;;
    gpg)
      local tmp="$AK_DIR/.tmp.$$"; : > "$tmp"
      [[ -f "$AK_VAULT" ]] && gpg --quiet --decrypt "$AK_VAULT" > "$tmp" 2>/dev/null || true
      ak_sed_inplace -e "/^${k}=.*/d" "$tmp"
      printf "%s=%s\n" "$k" "$(printf '%s' "$v" | base64 | tr -d '\n')" >> "$tmp"
      gpg --yes -o "$AK_VAULT" --symmetric --cipher-algo AES256 "$tmp" && rm -f "$tmp"
      ;;
  esac
}
ak_store_get() {
  ak_detect_backend
  local k="$1"
  case "$AK_BACKEND" in
    pass) pass show "ak/$k" 2>/dev/null || return 1 ;;
    secret-tool) secret-tool lookup ak key "$k" 2>/dev/null || return 1 ;;
    gpg)
      [[ -f "$AK_VAULT" ]] || return 1
      gpg --quiet --decrypt "$AK_VAULT" 2>/dev/null | "$AWK_BIN" -F= -v key="$k" '$1==key{print $2}' | head -n1 | base64 --decode 2>/dev/null || return 1
      ;;
  esac
}
ak_store_rm() {
  ak_detect_backend; ak_mkdirs
  local k="$1"
  case "$AK_BACKEND" in
    pass) pass rm -f "ak/$k" >/dev/null ;;
    secret-tool) secret-tool clear ak key "$k" >/dev/null ;;
    gpg)
      [[ -f "$AK_VAULT" ]] || return 0
      local tmp="$AK_DIR/.tmp.$$"
      gpg --quiet --decrypt "$AK_VAULT" > "$tmp" 2>/dev/null || true
      ak_sed_inplace -e "/^${k}=.*/d" "$tmp"
      gpg --yes -o "$AK_VAULT" --symmetric --cipher-algo AES256 "$tmp" && rm -f "$tmp"
      ;;
  esac
}
ak_store_list() {
  ak_detect_backend
  case "$AK_BACKEND" in
    pass) pass ls ak 2>/dev/null | sed -E 's/.*[├└│└─]+ //;s#ak/##;s#/$##' | grep -v '^ak$' ;;
    secret-tool) env | grep -E '(_API_KEY|_TOKEN)=' | cut -d= -f1 | "$SORT_BIN" -u ;;
    gpg) [[ -f "$AK_VAULT" ]] && gpg --quiet --decrypt "$AK_VAULT" 2>/dev/null | cut -d= -f1 | "$SORT_BIN" -u || true ;;
  esac
}

# --------- Profiles ----------
ak_profile_path() { printf '%s/%s.profile\n' "$AK_PROFILES_DIR" "$1"; }
ak_profile_save() {
  local prof="$1"; shift || true; ak_mkdirs
  local path; path="$(ak_profile_path "$prof")"
  if [[ "$#" -eq 0 ]]; then
    env | grep -E '(_API_KEY|_TOKEN)=' | cut -d= -f1 | "$SORT_BIN" -u >| "$path"
  else
    printf '%s\n' "$@" | "$SORT_BIN" -u >| "$path"
  fi
  ak_ok "Saved profile '$prof' ($(wc -l <"$path" | tr -d ' ') keys)."
}
ak_profile_ls() { [[ -d "$AK_PROFILES_DIR" ]] || return 0; find "$AK_PROFILES_DIR" -maxdepth 1 -type f -name '*.profile' -printf '%f\n' | sed 's/\.profile$//' | "$SORT_BIN"; }
ak_profile_use() {
  local prof="$1" path; path="$(ak_profile_path "$prof")"; [[ -f "$path" ]] || ak_err "Profile '$prof' not found."
  local missing=0 k v
  while IFS= read -r k; do
    [[ -z "$k" ]] && continue
    v="$(ak_store_get "$k" || true)"
    if [[ -n "$v" ]]; then export "$k=$v"; else ak_warn "$k not found in storage"; ((missing++)); fi
  done < "$path"
  ak_ok "Profile '$prof' loaded. Missing: $missing"
}
ak_profile_env() {
  local prof="$1" path; path="$(ak_profile_path "$prof")"; [[ -f "$path" ]] || ak_err "Profile '$prof' not found."
  local k v
  while IFS= read -r k; do
    [[ -z "$k" ]] && continue
    v="$(ak_store_get "$k" || true)"
    [[ -n "$v" ]] && printf 'export %s=%q\n' "$k" "$v"
  done < "$path"
}

# --------- Import/Export parsers ----------
ak_parse_env_file() {
  local file="$1"
  "$AWK_BIN" '
    BEGIN{FS="="}
    /^[[:space:]]*#/ {next}
    /^[[:space:]]*$/ {next}
    {
      line=$0
      sub(/^[[:space:]]*/,"",line)
      sub(/[[:space:]]*$/,"",line)
      sub(/^export[[:space:]]+/,"",line)
      split(line, arr, "=")
      key=arr[1]
      sub(/[[:space:]]*$/,"",key)
      val=substr(line, index(line, "=")+1)
      sub(/^[[:space:]]*/,"",val)
      if (match(val, /^"[^\r\n]*"$/))   { val=substr(val,2,length(val)-2); gsub(/\\"/,"\"",val); gsub(/\\\\/,"\\",val) }
      else if (match(val, /^'\''[^\r\n]*'\''$/)) { val=substr(val,2,length(val)-2) }
      print key "\t" val
    }
  ' "$file"
}
ak_parse_json_file() {
  local file="$1"
  if ak_has jq; then jq -r 'to_entries[] | "\(.key)\t\(.value)"' "$file"
  else sed -n 's/[[:space:]]*"([^"]+)"[[:space:]]*:[[:space:]]*"([^"]*)"[[:space:]]*,*/\1\t\2/p' "$file" 2>/dev/null
  fi
}
ak_parse_yaml_file() {
  local file="$1"
  "$AWK_BIN" -F: '
    /^[[:space:]]*#/ {next}
    /^[[:space:]]*$/ {next}
    /^[[:space:]]*[^:#][^:]*:[[:space:]]*/ {
      key=$1; sub(/^[[:space:]]*/,"",key); sub(/[[:space:]]*$/,"",key)
      val=$0; sub(/^[^:]*:[[:space:]]*/,"",val)
      sub(/^[[:space:]]*/,"",val); sub(/[[:space:]]*$/,"",val)
      if (match(val, /^"[^\r\n]*"$/)) { val=substr(val,2,length(val)-2); gsub(/\\"/,"\"",val); gsub(/\\\\/,"\\",val) }
      else if (match(val, /^'\''[^\r\n]*'\''$/)) { val=substr(val,2,length(val)-2) }
      print key "\t" val
    }
  ' "$file"
}
ak_export_profile() {
  local prof="$1" fmt="$2" out="$3"
  local path; path="$(ak_profile_path "$prof")"; [[ -f "$path" ]] || ak_err "Profile '$prof' not found."
  local tmp; tmp="$(mktemp)"; : > "$tmp"
  local exported=0
  case "$fmt" in
    env|dotenv)
      while IFS= read -r k; do
        [[ -z "$k" ]] && continue
        local v_store v_env v
        v_store="$(ak_store_get "$k" 2>/dev/null || true)"
        v_env="${!k:-}"
        v="${v_store:-$v_env}"
        printf "%s=\"%s\"\n" "$k" "$(printf '%s' "$v" | sed 's/"/\\"/g')" >> "$tmp"
        exported=$((exported+1))
      done < "$path"
      ;;
    json)
      printf '{' >> "$tmp"; local first=1
      while IFS= read -r k; do
        [[ -z "$k" ]] && continue
        local v_store v_env v
        v_store="$(ak_store_get "$k" 2>/dev/null || true)"
        v_env="${!k:-}"
        v="${v_store:-$v_env}"
        [[ $first -eq 1 ]] && first=0 || printf ',' >> "$tmp"
        printf '"%s":"%s"' "$k" "$(ak_json_escape "$v")" >> "$tmp"
        exported=$((exported+1))
      done < "$path"
      printf '}' >> "$tmp"
      ;;
    yaml)
      while IFS= read -r k; do
        [[ -z "$k" ]] && continue
        local v_store v_env v
        v_store="$(ak_store_get "$k" 2>/dev/null || true)"
        v_env="${!k:-}"
        v="${v_store:-$v_env}"
        local q; q="$(ak_shell_quote "$v")"; [[ "$q" == \"* ]] || q="\"$q\""
        printf "%s: %s\n" "$k" "$q" >> "$tmp"
        exported=$((exported+1))
      done < "$path"
      ;;
    *) ak_err "Unknown format: $fmt" ;;
  esac
  mv "$tmp" "$out"; ak_ok "Exported profile '$prof' → $fmt: $out ($exported keys)"
}
ak_import_file() {
  local prof="$1" fmt="$2" file="$3" append="${4:-0}"
  [[ -f "$file" ]] || ak_err "File not found: $file"
  ak_mkdirs
  local path; path="$(ak_profile_path "$prof")"
  [[ "$append" = "1" ]] || : > "$path"
  
  # Statistics tracking
  local processed=0 mapped=0 duplicated=0 skipped=0 final=0
  declare -A canonical_keys canonical_values seen_keys
  
  local reader
  case "$fmt" in
    env|dotenv) reader=ak_parse_env_file ;;
    json)       reader=ak_parse_json_file ;;
    yaml)       reader=ak_parse_yaml_file ;;
    *)          ak_err "Unknown format: $fmt" ;;
  esac
  
  # First pass: collect and analyze all keys
  while IFS=$'\t' read -r key val; do
    [[ -z "$key" ]] && continue
    processed=$((processed+1))
    
    local normalized_key; normalized_key="$(ak_normalize_key "$key")"
    
    # Track if key was mapped
    [[ "$key" != "$normalized_key" ]] && {
      ak_ok "Mapped $key → $normalized_key"
      mapped=$((mapped+1))
    }
    
    # Handle conflicts: load first valid value for each canonical key
    if [[ -n "${canonical_keys[$normalized_key]:-}" ]]; then
      # Key already seen - this is a duplicate
      duplicated=$((duplicated+1))
      ak_warn "Duplicate: $key → $normalized_key (keeping first valid value)"
    else
      # First time seeing this canonical key
      if [[ -n "$val" ]]; then
        canonical_keys[$normalized_key]="$key"
        canonical_values[$normalized_key]="$val"
        final=$((final+1))
      else
        ak_warn "Skipped: $key (empty value)"
        skipped=$((skipped+1))
      fi
    fi
  done < <("$reader" "$file")
  
  # Second pass: store valid keys
  for norm_key in "${!canonical_keys[@]}"; do
    ak_store_set "$norm_key" "${canonical_values[$norm_key]}"
    printf '%s\n' "$norm_key" >> "$path"
  done
  
  "$SORT_BIN" -u "$path" -o "$path"
  
  # Detailed report
  ak_ok "Import Analysis:"
  ak_ok "  Processed: $processed keys"
  ak_ok "  Mapped: $mapped keys"
  ak_ok "  Duplicated: $duplicated keys"
  ak_ok "  Skipped: $skipped keys (empty values)"
  ak_ok "  Final: $final unique keys stored in profile '$prof'"
}

# --------- Clipboard ----------
ak_clip_copy() {
  local text="$1"
  if ak_has pbcopy; then
    printf '%s' "$text" | pbcopy; ( sleep "$AK_CLIP_TTL"; printf '' | pbcopy ) >/dev/null 2>&1 &
  elif ak_has wl-copy; then
    printf '%s' "$text" | wl-copy; ( sleep "$AK_CLIP_TTL"; wl-copy </dev/null ) >/dev/null 2>&1 &
  elif ak_has xclip; then
    printf '%s' "$text" | xclip -selection clipboard; ( sleep "$AK_CLIP_TTL"; printf '' | xclip -selection clipboard ) >/dev/null 2>&1 &
  else
    return 1
  fi
}

# --------- Curl & service tests ----------
ak_curl() { curl -sS -f -L --connect-timeout 5 --max-time 12 "$@"; }
ak_test_one() {
  local svc="$1" ok=0 body k
  case "$svc" in
    anthropic)  k="$(ak_store_get ANTHROPIC_API_KEY || echo "${ANTHROPIC_API_KEY:-}")"; [[ -n "$k" ]] || return 4; body="$(ak_curl -X POST https://api.anthropic.com/v1/messages -H "x-api-key: $k" -H "anthropic-version: 2023-06-01" -H "content-type: application/json" -d '{"model":"claude-3-haiku-20240307","max_tokens":4,"messages":[{"role":"user","content":"ping"}]}')" && ok=1 ;;
    azure_openai) k="$(ak_store_get AZURE_OPENAI_API_KEY || echo "${AZURE_OPENAI_API_KEY:-}")"; [[ -n "$k" && -n "${AZURE_OPENAI_ENDPOINT:-}" ]] || return 4; body="$(ak_curl -H "api-key: $k" "$AZURE_OPENAI_ENDPOINT/openai/deployments?api-version=2024-02-15-preview")" && ok=1 ;;
    brave)      k="$(ak_store_get BRAVE_API_KEY      || echo "${BRAVE_API_KEY:-}")";      [[ -n "$k" ]] || return 4; body="$(ak_curl -H "X-Subscription-Token: $k" "https://api.search.brave.com/res/v1/web/search?q=ping")" && ok=1 ;;
    cohere)     k="$(ak_store_get COHERE_API_KEY     || echo "${COHERE_API_KEY:-}")";     [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://api.cohere.com/v1/models)" && ok=1 ;;
    deepseek)   k="$(ak_store_get DEEPSEEK_API_KEY   || echo "${DEEPSEEK_API_KEY:-}")";   [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://api.deepseek.com/v1/models)" && ok=1 ;;
    exa)        k="$(ak_store_get EXA_API_KEY        || echo "${EXA_API_KEY:-}")";        [[ -n "$k" ]] || return 4; body="$(ak_curl -X POST https://api.exa.ai/search -H "x-api-key: $k" -H "content-type: application/json" -d '{"query":"ping","numResults":1}')" && ok=1 ;;
    fireworks)  k="$(ak_store_get FIREWORKS_API_KEY  || echo "${FIREWORKS_API_KEY:-}")";  [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://api.fireworks.ai/inference/v1/models)" && ok=1 ;;
    gemini)     k="$(ak_store_get GEMINI_API_KEY     || echo "${GOOGLE_GENAI_API_KEY:-${GOOGLE_API_KEY:-${GEMINI_API_KEY:-}}}")"; [[ -n "$k" ]] || return 4; body="$(ak_curl "https://generativelanguage.googleapis.com/v1beta/models?key=$k")" && ok=1 ;;
    groq)       k="$(ak_store_get GROQ_API_KEY       || echo "${GROQ_API_KEY:-}")";       [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://api.groq.com/openai/v1/models)" && ok=1 ;;
    huggingface)k="$(ak_store_get HUGGINGFACE_TOKEN  || echo "${HUGGINGFACE_TOKEN:-}")";  [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://huggingface.co/api/whoami-v2)" && ok=1 ;;
    mistral)    k="$(ak_store_get MISTRAL_API_KEY    || echo "${MISTRAL_API_KEY:-}")";    [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://api.mistral.ai/v1/models)" && ok=1 ;;
    openai)     k="$(ak_store_get OPENAI_API_KEY     || echo "${OPENAI_API_KEY:-}")";     [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://api.openai.com/v1/models)" && ok=1 ;;
    openrouter) k="$(ak_store_get OPENROUTER_API_KEY || echo "${OPENROUTER_API_KEY:-}")"; [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://openrouter.ai/api/v1/models)" && ok=1 ;;
    perplexity) k="$(ak_store_get PERPLEXITY_API_KEY || echo "${PERPLEXITY_API_KEY:-}")"; [[ -n "$k" ]] || return 4; body="$(ak_curl -X POST https://api.perplexity.ai/chat/completions -H "Authorization: Bearer $k" -H "Content-Type: application/json" -d '{"model":"sonar-small-chat","messages":[{"role":"user","content":"hello"}],"max_tokens":4}')" && ok=1 ;;
    sambanova)  k="$(ak_store_get SAMBANOVA_API_KEY  || echo "${SAMBANOVA_API_KEY:-}")";  [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://api.sambanova.ai/v1/models)" && ok=1 ;;
    tavily)     k="$(ak_store_get TAVILY_API_KEY     || echo "${TAVILY_API_KEY:-}")";     [[ -n "$k" ]] || return 4; body="$(ak_curl -X POST https://api.tavily.com/search -H "Content-Type: application/json" -d "{\"api_key\":\"$k\",\"query\":\"ping\"}")" && ok=1 ;;
    together)   k="$(ak_store_get TOGETHER_API_KEY   || echo "${TOGETHER_API_KEY:-}")";   [[ -n "$k" ]] || return 4; body="$(ak_curl -H "Authorization: Bearer $k" https://api.together.ai/v1/models)" && ok=1 ;;
    xai)        k="$(ak_store_get XAI_API_KEY        || echo "${XAI_API_KEY:-}")";        [[ -n "$k" ]] || return 4; body="$(ak_curl -X POST https://api.x.ai/v1/chat/completions -H "Authorization: Bearer $k" -H "Content-Type: application/json" -d '{"model":"grok-1","messages":[{"role":"user","content":"hello"}],"max_tokens":4}')" && ok=1 ;;
  esac
  return $(( ok ? 0 : 2 ))
}

# --------- Commands ----------
ak_help() {
cat <<'HLP'
AK — Secure API key manager (standalone)
USAGE:
  ak set <NAME>
  ak get <NAME> [--full]
  ak cp <NAME>
  ak ls
  ak rm <NAME>
  ak unset <NAME>              # remove from current env only
  ak search <PATTERN>
  ak test <service>|--all [--json] [--fail-fast]
  ak services
  ak save <profile> [NAMES...]
  ak load <profile> [--persist]  # load profile into current shell
  ak unload [profile]            # unload profile and remove persistence
  ak env --profile <p>           # print export lines (load: eval "$(ak env --profile p)")
  ak export --profile <p> --format env|dotenv|json|yaml --output <file>
  ak import --profile <p> --format env|dotenv|json|yaml --file <file> [--append]
  ak profiles
  ak edit <profile>
  ak guard enable|disable
  ak backend
  ak migrate exports <file>
  ak run --profile <p> -- <cmd...>
  ak uninstall                   # completely remove ak CLI and data
  ak doctor
HLP
}

ak_cmd() {
  local cmd="${1:-help}"; shift || true

  # global flags
  while [[ "${1:-}" == "--json" || "${1:-}" == "--help" || "${1:-}" == "-h" ]]; do
    case "$1" in
      --json) AK_JSON=1; shift ;;
      --help|-h) cmd=help; shift ;;
    esac
  done

  case "$cmd" in
    help) ak_help ;;
    backend) ak_detect_backend; printf '%s\n' "$AK_BACKEND" ;;
    set)
      local name="${1:-}"; [[ -n "$name" ]] || ak_err "Usage: ak set <NAME>"
      local value
      if [[ -z "${AK_NONINTERACTIVE:-}" ]]; then
        read -rsp "Enter value for $name: " value && printf '\n'
      else
        value="${AK_VALUE:-}"; [[ -n "$value" ]] || ak_err "Set AK_VALUE when AK_NONINTERACTIVE=1"
      fi
      [[ -n "$value" ]] || ak_err "Empty value, aborting."
      ak_store_set "$name" "$value"; ak_ok "Stored $name."
      ;;
    get)
      local name="${1:-}"; [[ -n "$name" ]] || ak_err "Usage: ak get <NAME> [--full]"
      shift || true
      local full="${1:-}"; local v; v="$(ak_store_get "$name" || echo "${!name:-}")"
      [[ -n "$v" ]] || ak_err "$name not found."
      if [[ "$full" == "--full" ]]; then printf '%s\n' "$v"; else ak_mask "$v"; fi
      ;;
    cp)
      local name="${1:-}"; [[ -n "$name" ]] || ak_err "Usage: ak cp <NAME>"
      local v; v="$(ak_store_get "$name" || echo "${!name:-}")"; [[ -n "$v" ]] || ak_err "$name not found."
      ak_clip_copy "$v" && ak_ok "Copied $name to clipboard (clears in $AK_CLIP_TTL s)." || ak_err "No clipboard utility (pbcopy/wl-copy/xclip)."
      ;;
    ls)
      if [[ "$AK_JSON" = 1 ]]; then
        printf '['; local first=1
        while read -r k; do
          [[ -z "$k" ]] && continue
          local val="${!k:-}" masked=""
          [[ -n "$val" ]] && masked="$(ak_mask "$val")"
          [[ $first -eq 1 ]] && first=0 || printf ','
          printf '{"name":"%s","in_env":%s%s}' "$k" $([[ -n "$val" ]] && echo true || echo false) $([[ -n "$val" ]] && printf ',"masked":"%s"' "$masked" || echo '')
        done < <(ak_store_list)
        printf ']\n'
      else
        while read -r k; do
          [[ -z "$k" ]] && continue
          local val="${!k:-}"
          if [[ -n "$val" ]]; then printf "%-34s %s\n" "$k" "$(ak_mask "$val")"; else printf "%-34s %s\n" "$k" "(stored)"; fi
        done < <(ak_store_list)
      fi
      ;;
    rm)
      local name="${1:-}"; [[ -n "$name" ]] || ak_err "Usage: ak rm <NAME>"
      if [[ -z "${AK_NONINTERACTIVE:-}" ]]; then
        read -rp "Delete $name from storage? [y/N] " yn; [[ "$yn" == "y" || "$yn" == "Y" ]] || ak_err "Aborted."
      fi
      ak_store_rm "$name"; ak_ok "Removed $name from storage."
      ;;
    unset)
      local name="${1:-}"; [[ -n "$name" ]] || ak_err "Usage: ak unset <NAME>"
      unset "$name" 2>/dev/null || true; ak_ok "Unset $name from environment."
      ;;
    search)
      local pat="${1:-}"; [[ -n "$pat" ]] || ak_err "Usage: ak search <PATTERN>"
      { ak_store_list; env | grep -E '(_API_KEY|_TOKEN)=' | cut -d= -f1; } | "$SORT_BIN" -u | grep -i -- "$pat" || true
      ;;
    services) printf '%s\n' "${!AK_SERVICE_KEYS[@]}" | "$SORT_BIN" ;;
    test)
      local svc="${1:-}"; shift || true
      local all=0 ff=0
      [[ "$svc" == "--all" || "$svc" == "all" ]] && { all=1; }
      [[ "${1:-}" == "--fail-fast" ]] && ff=1 || true
      if (( all==1 )); then
        local ok_all=0; ok_all=1
        [[ "$AK_JSON" = 1 ]] && printf '['
        local first=1
        for s in $(printf '%s\n' "${!AK_SERVICE_KEYS[@]}" | "$SORT_BIN"); do
          local key="${AK_SERVICE_KEYS[$s]}"; local have="${!key:-}"; [[ -z "$have" ]] && have="$(ak_store_get "$key" || true)"
          [[ -z "$have" ]] && continue
          if ak_test_one "$s"; then
            [[ "$AK_JSON" = 1 ]] && { [[ $first -eq 1 ]] && first=0 || printf ','; printf '{"service":"%s","ok":true}' "$s"; } || ak_ok "$s OK"
          else
            ok_all=0
            [[ "$AK_JSON" = 1 ]] && { [[ $first -eq 1 ]] && first=0 || printf ','; printf '{"service":"%s","ok":false}' "$s"; } || ak_warn "$s failed"
            (( ff==1 )) && break
          fi
        done
        [[ "$AK_JSON" = 1 ]] && printf ']\n'
        exit $(( ok_all ? 0 : 2 ))
      else
        [[ -n "$svc" ]] || ak_err "Usage: ak test <service>|--all [--fail-fast] [--json]"
        if ak_test_one "$svc"; then [[ "$AK_JSON" = 1 ]] && printf '{"service":"%s","ok":true}\n' "$svc" || ak_ok "$svc OK"; else [[ "$AK_JSON" = 1 ]] && printf '{"service":"%s","ok":false}\n' "$svc" || ak_err "$svc failed"; fi
      fi
      ;;
    save)
      local prof="${1:-}"; shift || true; [[ -n "$prof" ]] || ak_err "Usage: ak save <profile> [NAMES...]"
      ak_profile_save "$prof" "$@"
      ;;
    load)
      local prof="${1:-default}"
      # Output export statements for eval
      ak_profile_env "$prof"
      ;;
    unload)
      ak_err "Use shell integration: ak unload [profile]"
      ;;
    uninstall)
      echo "🗑️  Uninstalling ak CLI..."
      # Remove shell integration from config files
      for config in "$HOME/.zshrc" "$HOME/.bashrc" "$HOME/.profile"; do
        if [[ -f "$config" ]]; then
          # Remove shell integration line
          if grep -q "shell-init.sh" "$config" 2>/dev/null; then
            sed -i '/shell-init\.sh/d' "$config"
            echo "✅ Removed shell integration from $config"
          fi
          # Remove PATH line added by ak installer
          if grep -q "# Added by ak installer" "$config" 2>/dev/null; then
            sed -i '/# Added by ak installer/,+1d' "$config"
            echo "✅ Removed PATH configuration from $config"
          fi
        fi
      done
      
      # Remove binary and data
      rm -f "$0" 2>/dev/null || true
      rm -rf "$(dirname "$AK_VAULT")" 2>/dev/null || true
      # Remove shell integration files and persist data
      rm -rf "$HOME/.local/share/ak" 2>/dev/null || true
      # remove the bin
      rm -f "$HOME/.local/bin/ak" 2>/dev/null || true
      echo "✅ Removed ak data directory and shell integration files"
      echo "✅ ak CLI completely uninstalled. Restart your terminal."
      ;;
    env)
      [[ "${1:-}" == "--profile" ]] || ak_err "Usage: ak env --profile <name>"
      shift; local prof="${1:-}"; [[ -n "$prof" ]] || ak_err "Usage: ak env --profile <name>"
      ak_profile_env "$prof"
      ;;
    export)
      local prof fmt out
      while [[ -n "${1:-}" ]]; do
        case "$1" in
          --profile) prof="$2"; shift 2 ;;
          --format)  fmt="$2"; shift 2 ;;
          --output)  out="$2"; shift 2 ;;
          *) ak_err "Unknown flag '$1'";;
        esac
      done
      # Set defaults
      prof="${prof:-default}"
      fmt="${fmt:-dotenv}"
      [[ -n "${out:-}" ]] || ak_err "Usage: ak export [--profile <name>] [--format env|dotenv|json|yaml] --output <file>"
      ak_export_profile "$prof" "$fmt" "$out"
      ;;
    import)
      local prof fmt file append=0
      while [[ -n "${1:-}" ]]; do
        case "$1" in
          --profile) prof="$2"; shift 2 ;;
          --format)  fmt="$2"; shift 2 ;;
          --file)    file="$2"; shift 2 ;;
          --append)  append=1; shift ;;
          *) ak_err "Unknown flag '$1'";;
        esac
      done
      # Set defaults
      prof="${prof:-default}"
      fmt="${fmt:-dotenv}"
      [[ -n "${file:-}" ]] || ak_err "Usage: ak import [--profile <name>] [--format env|dotenv|json|yaml] --file <file> [--append]"
      ak_import_file "$prof" "$fmt" "$file" "$append"
      ;;
    profiles) ak_profile_ls ;;
    edit)
      local prof="${1:-}"; [[ -n "$prof" ]] || ak_err "Usage: ak edit <profile>"
      ak_mkdirs; local path; path="$(ak_profile_path "$prof")"; [[ -f "$path" ]] || : > "$path"; "${EDITOR:-vi}" "$path"
      ;;
    guard)
      local sub="${1:-}"; [[ -n "$sub" ]] || ak_err "Usage: ak guard enable|disable"
      local hook; hook="$(git rev-parse --git-path hooks/pre-commit 2>/dev/null || true)"; [[ -n "$hook" ]] || ak_err "Not a git repo."
      case "$sub" in
        enable)
          mkdir -p "$(dirname "$hook")"
          cat >| "$hook" <<'HOOK'
#!/usr/bin/env bash
set -euo pipefail
files=$(git diff --cached --name-only --diff-filter=ACM)
[ -z "$files" ] && exit 0
if command -v gitleaks >/dev/null 2>&1; then
  gitleaks protect --staged
else
  if grep -EIHn --exclude-dir=.git -- $'AKIA|ASIA|ghp_[A-Za-z0-9]{36}|xox[baprs]-|-----BEGIN (PRIVATE|OPENSSH PRIVATE) KEY-----|api_key|_API_KEY|_TOKEN' $files; then
    echo "ak guard: possible secrets detected in staged files above."
    echo "Commit aborted. Override with: git commit -n"
    exit 1
  fi
fi
exit 0
HOOK
          chmod +x "$hook"; ak_ok "Installed pre-commit secret guard."
          ;;
        disable)
          [[ -f "$hook" ]] || { ak_warn "No guard installed."; exit 0; }
          rm -f "$hook"; ak_ok "Removed pre-commit secret guard."
          ;;
        *) ak_err "Usage: ak guard enable|disable" ;;
      esac
      ;;
    migrate)
      [[ "${1:-}" == "exports" ]] || ak_err "Usage: ak migrate exports <file>"
      local file="${2:-}"; [[ -n "$file" && -f "$file" ]] || ak_err "File not found"
      ak_detect_backend; ak_mkdirs
      "$AWK_BIN" -F'[ =]+' '/^export[[:space:]]+[A-Z0-9_]+=/ {print $2}' "$file" | while read -r k; do
        local line val
        line="$(grep -E "^export[[:space:]]+$k=" "$file" | tail -n1)"
        val="$(printf '%s' "$line" | sed -E 's/^export[[:space:]]+'"$k"'=//' | sed -E "s/^['\"](.*)['\"]$/\1/")"
        [[ -n "$val" && "$val" != "omitted" ]] || continue
        ak_store_set "$k" "$val" && ak_ok "Imported $k"
      done
      ak_ok "Migration complete."
      ;;
    run)
      [[ "${1:-}" == "--profile" ]] || ak_err "Usage: ak run --profile <name> -- <cmd...>"
      shift; local prof="${1:-}"; shift; [[ "${1:-}" == "--" ]] || ak_err "Usage: ak run --profile <name> -- <cmd...>"; shift
      local exports; exports="$(ak_profile_env "$prof")"
      # shellcheck disable=SC2046
      env $(printf '%s\n' "$exports") "$@"
      ;;
    doctor)
      ak_detect_backend
      printf 'backend: %s\n' "$AK_BACKEND"
      for b in pass secret-tool gpg; do ak_has "$b" && printf 'found: %s\n' "$b"; done
      for c in pbcopy wl-copy xclip; do ak_has "$c" && printf 'clipboard: %s\n' "$c"; done
      ak_has gitleaks && printf 'scanner: gitleaks\n' || true
      printf 'profiles: %s\n' "$(ak_profile_ls | wc -l | tr -d ' ')"
      ;;
    *) ak_err "Unknown command '$cmd' (try: ak help)" ;;
  esac
}

ak_cmd "$@"
AKCLI

chmod +x "$AK_PATH"

# Create shell integration script
SHELL_INIT_FILE="$PREFIX/share/ak/shell-init.sh"
mkdir -p "$(dirname "$SHELL_INIT_FILE")"
cat > "$SHELL_INIT_FILE" <<SHELL_INIT
# ak shell integration - add this to your shell profile
export AK_DATA_DIR="$PREFIX/share/ak"

ak() {
  if [[ "\$1" == "load" ]]; then
    local prof="\${2:-default}"
    local persist=0
    
    # Check for --persist flag
    if [[ "\${3:-}" == "--persist" ]] || [[ "\${2:-}" == "--persist" ]]; then
      persist=1
      [[ "\$2" == "--persist" ]] && prof="default"
    fi
    
    local exports
    exports="\$("$AK_PATH" env --profile "\$prof" 2>/dev/null)"
    if [[ \$? -eq 0 ]]; then
      eval "\$exports"
      
      if [[ \$persist -eq 1 ]]; then
        # Create encrypted bundle in ak persist directory
        local persist_dir="\$AK_DATA_DIR/persist"
        mkdir -p "\$persist_dir"
        
        # Encrypt all exports as one blob for fast loading
        local encrypted_bundle
        encrypted_bundle="\$(printf '%s\n' "\$exports" | openssl enc -aes-256-cbc -base64 -pass pass:ak-persist-\$USER 2>/dev/null)"
        
        if [[ -n "\$encrypted_bundle" ]]; then
          # Store encrypted bundle for this profile
          echo "\$encrypted_bundle" > "\$persist_dir/\$prof.bundle"
          
          # Create/update directory mapping for current path
          local current_path="\$(pwd)"
          local path_hash="\$(echo "\$current_path" | sha256sum | cut -d' ' -f1)"
          echo "\$prof:\$current_path" > "\$persist_dir/\$path_hash.map"
          
          # Mark keys as loaded for this session
          export AK_KEYS_LOADED="\$prof"
          
          echo "✅ Profile '\$prof' loaded into current shell and persisted for this directory"
          echo "   Bundle: \$persist_dir/\$prof.bundle"
          echo "   Path mapping: \$current_path -> \$prof"
        else
          echo "❌ Failed to encrypt profile bundle"
          return 1
        fi
      else
        # Mark keys as loaded for this session
        export AK_KEYS_LOADED="\$prof"
        echo "✅ Profile '\$prof' loaded into current shell."
      fi
    else
      echo "❌ Failed to load profile '\$prof'"
      return 1
    fi
  elif [[ "\$1" == "unload" ]]; then
    local prof="\${2:-\$AK_KEYS_LOADED}"
    [[ -n "\$prof" ]] || { echo "❌ No profile currently loaded"; return 1; }
    
    # Remove persistent mapping if it exists
    local persist_dir="\$AK_DATA_DIR/persist"
    if [[ -d "\$persist_dir" ]]; then
      local current_path="\$(pwd)"
      local path_hash="\$(echo "\$current_path" | sha256sum | cut -d' ' -f1 2>/dev/null)"
      local map_file="\$persist_dir/\$path_hash.map"
      
      if [[ -f "\$map_file" ]]; then
        local profile_info="\$(cat "\$map_file" 2>/dev/null)"
        local mapped_profile="\${profile_info%%:*}"
        
        if [[ "\$mapped_profile" == "\$prof" ]]; then
          rm -f "\$map_file"
          echo "🗑️  Removed persistent mapping for '\$prof' in this directory"
        fi
      fi
    fi
    
    # Unset all profile environment variables
    "$AK_PATH" env --profile "\$prof" 2>/dev/null | sed 's/^export //' | cut -d'=' -f1 | while read -r key; do
      [[ -n "\$key" ]] && unset "\$key"
    done
    
    unset AK_KEYS_LOADED
    echo "✅ Profile '\$prof' unloaded from current shell"
  else
    "$AK_PATH" "\$@"
  fi
}

# Auto-load persisted profile for current directory
ak_auto_load() {
  [[ -n "\${AK_DATA_DIR:-}" ]] || return 0
  command -v openssl >/dev/null 2>&1 || return 0
  
  local persist_dir="\$AK_DATA_DIR/persist"
  [[ -d "\$persist_dir" ]] || return 0
  
  local current_path="\$(pwd)"
  local path_hash="\$(echo "\$current_path" | sha256sum | cut -d' ' -f1 2>/dev/null)"
  [[ -n "\$path_hash" ]] || return 0
  
  local map_file="\$persist_dir/\$path_hash.map"
  [[ -f "\$map_file" ]] || return 0
  
  local profile_info="\$(cat "\$map_file" 2>/dev/null)"
  local profile_name="\${profile_info%%:*}"
  local mapped_path="\${profile_info#*:}"
  
  # Only auto-load if we're in the exact path that was persisted
  [[ "\$current_path" == "\$mapped_path" ]] || return 0
  
  local bundle_file="\$persist_dir/\$profile_name.bundle"
  [[ -f "\$bundle_file" ]] || return 0
  
  # Load encrypted bundle
  local encrypted_bundle="\$(cat "\$bundle_file" 2>/dev/null)"
  [[ -n "\$encrypted_bundle" ]] || return 0
  
  eval "\$(echo "\$encrypted_bundle" | openssl enc -aes-256-cbc -base64 -d -pass pass:ak-persist-\$USER 2>/dev/null || true)"
  export AK_KEYS_LOADED="\$profile_name"
}

# Auto-load on shell startup (if not already loaded)
[[ -n "\${AK_KEYS_LOADED:-}" ]] || ak_auto_load

# Auto-load when changing directories
if command -v chpwd >/dev/null 2>&1; then
  # zsh hook
  chpwd() { ak_auto_load; }
elif [[ -n "\${BASH_VERSION:-}" ]]; then
  # bash hook using PROMPT_COMMAND
  if [[ "\${PROMPT_COMMAND:-}" != *"ak_auto_load"* ]]; then
    PROMPT_COMMAND="ak_auto_load;\${PROMPT_COMMAND:-}"
  fi
fi

# Shell autocompletion
_ak_completion() {
  local cur prev words cword
  if [[ -n "\${BASH_VERSION:-}" ]]; then
    # Bash completion
    COMPREPLY=()
    cur="\${COMP_WORDS[COMP_CWORD]}"
    prev="\${COMP_WORDS[COMP_CWORD-1]}"
    words=("\${COMP_WORDS[@]}")
    cword=\$COMP_CWORD
  elif [[ -n "\${ZSH_VERSION:-}" ]]; then
    # Zsh completion - set defaults if CURRENT is not set
    local current=\${CURRENT:-1}
    cur="\${words[\$current]}"
    prev="\${words[\$((current > 1 ? current-1 : 1))]}"
    cword=\$((current-1))
  fi
  
  local commands="load unload set get cp ls rm unset search test services save env export import profiles edit guard backend migrate run doctor help"
  local flags="--persist --full --json --help -h --fail-fast --append"
  local formats="env dotenv json yaml"
  
  case "\${prev}" in
    ak)
      if [[ -n "\${BASH_VERSION:-}" ]]; then
        COMPREPLY=(\$(compgen -W "\$commands" -- "\$cur"))
      elif [[ -n "\${ZSH_VERSION:-}" ]]; then
        _describe 'commands' \$(echo \$commands | tr ' ' '\n' | sed 's/^/ak_/')
        return 0
      fi
      ;;
    load|unload|save|edit)
      # Complete with available profiles
      local profiles=\$("$AK_PATH" profiles 2>/dev/null || echo "default")
      if [[ -n "\${BASH_VERSION:-}" ]]; then
        COMPREPLY=(\$(compgen -W "\$profiles \$flags" -- "\$cur"))
      elif [[ -n "\${ZSH_VERSION:-}" ]]; then
        _describe 'profiles' \$(echo \$profiles | tr ' ' '\n')
        _describe 'flags' \$(echo \$flags | tr ' ' '\n')
        return 0
      fi
      ;;
    test)
      # Complete with service names + --all
      local services="anthropic brave cohere deepseek exa fireworks gemini groq huggingface mistral openai openrouter perplexity sambanova tavily together xai"
      if [[ -n "\${BASH_VERSION:-}" ]]; then
        COMPREPLY=(\$(compgen -W "\$services --all --json --fail-fast" -- "\$cur"))
      elif [[ -n "\${ZSH_VERSION:-}" ]]; then
        _describe 'services' \$(echo \$services | tr ' ' '\n')
        _describe 'flags' \$(echo "--all --json --fail-fast" | tr ' ' '\n')
        return 0
      fi
      ;;
    --format)
      if [[ -n "\${BASH_VERSION:-}" ]]; then
        COMPREPLY=(\$(compgen -W "\$formats" -- "\$cur"))
      elif [[ -n "\${ZSH_VERSION:-}" ]]; then
        _describe 'formats' \$(echo \$formats | tr ' ' '\n')
        return 0
      fi
      ;;
    --profile)
      local profiles=\$("$AK_PATH" profiles 2>/dev/null || echo "default")
      if [[ -n "\${BASH_VERSION:-}" ]]; then
        COMPREPLY=(\$(compgen -W "\$profiles" -- "\$cur"))
      elif [[ -n "\${ZSH_VERSION:-}" ]]; then
        _describe 'profiles' \$(echo \$profiles | tr ' ' '\n')
        return 0
      fi
      ;;
    export|import)
      if [[ -n "\${BASH_VERSION:-}" ]]; then
        COMPREPLY=(\$(compgen -W "--profile --format --output --file --append" -- "\$cur"))
      elif [[ -n "\${ZSH_VERSION:-}" ]]; then
        _describe 'flags' \$(echo "--profile --format --output --file --append" | tr ' ' '\n')
        return 0
      fi
      ;;
    --output|--file)
      # Complete with filenames
      if [[ -n "\${BASH_VERSION:-}" ]]; then
        COMPREPLY=(\$(compgen -f -- "\$cur"))
      elif [[ -n "\${ZSH_VERSION:-}" ]]; then
        _files
        return 0
      fi
      ;;
    *)
      # Default to common flags
      if [[ "\$cur" == -* ]]; then
        if [[ -n "\${BASH_VERSION:-}" ]]; then
          COMPREPLY=(\$(compgen -W "\$flags" -- "\$cur"))
        elif [[ -n "\${ZSH_VERSION:-}" ]]; then
          _describe 'flags' \$(echo \$flags | tr ' ' '\n')
          return 0
        fi
      fi
      ;;
  esac
  
  return 0
}

# Register completion
if [[ -n "\${BASH_VERSION:-}" ]]; then
  complete -F _ak_completion ak
elif [[ -n "\${ZSH_VERSION:-}" ]]; then
  compdef _ak_completion ak
fi

SHELL_INIT

# Auto-configure shell integration
auto_configure_shell() {
  local shell_config=""
  local shell_name="$(basename "$SHELL" 2>/dev/null || echo "bash")"
  
  # Determine shell config file
  case "$shell_name" in
    zsh)  shell_config="$HOME/.zshrc" ;;
    bash) shell_config="$HOME/.bashrc" ;;
    fish) shell_config="$HOME/.config/fish/config.fish" ;;
    *)    shell_config="$HOME/.profile" ;;
  esac
  
  # Create config file if it doesn't exist
  [[ -f "$shell_config" ]] || touch "$shell_config"
  
  # Check and add PATH if not already present
  if ! grep -q "$BIN_DIR" "$shell_config" 2>/dev/null; then
    echo "" >> "$shell_config"
    echo "# Added by ak installer" >> "$shell_config"
    echo "export PATH=\"$BIN_DIR:\$PATH\"" >> "$shell_config"
    echo "✅ Added $BIN_DIR to PATH in $shell_config"
  else
    echo "✅ PATH already configured in $shell_config"
  fi
  
  # Check and add shell integration if not already present
  if ! grep -q "shell-init.sh" "$shell_config" 2>/dev/null; then
    echo "source \"$SHELL_INIT_FILE\"" >> "$shell_config"
    echo "✅ Added ak shell integration to $shell_config"
  else
    echo "✅ Shell integration already configured in $shell_config"
  fi
  
  return 0
}

# Run auto-configuration
auto_configure_shell

# Create default profile if it doesn't exist
AK_PROFILES_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/ak/profiles"
if [[ ! -f "$AK_PROFILES_DIR/default.profile" ]]; then
  "$AK_PATH" save default 2>/dev/null || {
    # Fallback: create empty default profile
    mkdir -p "$AK_PROFILES_DIR"
    touch "$AK_PROFILES_DIR/default.profile"
  }
  echo "✅ Created default profile 'default'"
fi

# print summary
echo ""
echo "🎉 ak CLI installation complete!"
echo "Installed: $AK_PATH"
echo "Shell integration: Automatically configured"
echo ""
echo "Restart your terminal or run: source $(basename "$SHELL" 2>/dev/null | sed 's/.*\///' | awk '{print ($1=="zsh") ? "~/.zshrc" : ($1=="bash") ? "~/.bashrc" : "~/.profile"}')"
echo ""
echo "Then test with:"
echo "  ak load default"
echo "  ak help"
