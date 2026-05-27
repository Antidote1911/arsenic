#!/usr/bin/env bash
# =============================================================================
#  git.sh — Assistant Git Universel (Rust & C++)
#  Usage rapide    : ./git.sh "mon message de commit"
#  Menu interactif : ./git.sh
# =============================================================================

set -euo pipefail
IFS=$'\n\t'

# ── Couleurs ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

info()    { echo -e "${CYAN}ℹ ${RESET}$*"; }
success() { echo -e "${GREEN}✔ ${RESET}$*"; }
warn()    { echo -e "${YELLOW}⚠ ${RESET}$*"; }
error()   { echo -e "${RED}✖ ${RESET}$*" >&2; }
title()   { echo -e "\n${BOLD}${BLUE}━━━ $* ━━━${RESET}\n"; }
sep()     { echo -e "${BLUE}────────────────────────────────────────${RESET}"; }

confirm() {
  read -rp "$(echo -e "${YELLOW}? ${RESET}${1:-Confirmer ?} [o/N] ")" ans
  [[ "${ans,,}" =~ ^(o|oui|y|yes)$ ]]
}

require_git() {
  if ! git rev-parse --git-dir &>/dev/null; then
    error "Ce dossier n'est pas un dépôt Git."
    exit 1
  fi
}

# ── Gestion des dossiers à ignorer ───────────────────────────────────────────
IGNORE_PATTERNS=("target/" "build/" "node_modules/" ".env" "dist/" "__pycache__/" ".claude/" ".idea/")

setup_gitignore() {
  [[ ! -f .gitignore ]] && touch .gitignore
  for pattern in "${IGNORE_PATTERNS[@]}"; do
    if ! grep -qxF "$pattern" .gitignore 2>/dev/null; then
      printf '%s\n' "$pattern" >> .gitignore
      info "Ajouté au .gitignore : $pattern"
    fi
  done
}

untrack_ignored() {
  for pattern in "${IGNORE_PATTERNS[@]}"; do
    local dir="${pattern%/}"
    if git ls-files -- "$dir" 2>/dev/null | grep -q .; then
      warn "Retrait de l'index : ${pattern}"
      git rm -r --cached "$dir" 2>/dev/null || true
    fi
  done
}

# ── Stage intelligent ─────────────────────────────────────────────────────────
smart_add() {
  set +e
  local excludes=()
  for p in "${IGNORE_PATTERNS[@]}"; do
    excludes+=(":!${p}" ":!${p}**")
  done
  if git add . -- "${excludes[@]}" 2>/dev/null; then
    success "Fichiers stagés (dossiers ignorés exclus)."
  else
    git add -A
    for p in "${IGNORE_PATTERNS[@]}"; do
      git reset -- "$p" "${p}**" >/dev/null 2>&1 || true
    done
    success "Fichiers stagés (fallback)."
  fi
  set -e
}

# ── Bandeau branche ───────────────────────────────────────────────────────────
show_branch_banner() {
  local branch
  branch=$(git symbolic-ref --short HEAD 2>/dev/null || echo "HEAD détachée")
  local dirty="" dirty_visible=""
  if ! git diff --quiet 2>/dev/null; then dirty=" ${RED}●${RESET}"; dirty_visible=" ●"; fi
  local remote
  remote=$(git remote get-url origin 2>/dev/null | sed 's/.*github.com[:/]//' | sed 's/\.git$//' || echo "")

  local label="${branch}${dirty_visible}"
  local pad=$(( 34 - ${#label} ))
  (( pad < 0 )) && pad=0
  local spaces
  printf -v spaces '%*s' "$pad" ''

  echo ""
  echo -e "${BOLD}${BLUE}╔══════════════════════════════════════╗${RESET}"
  echo -e "${BOLD}${BLUE}║${RESET}  🌿 ${branch}${dirty}${spaces}${BOLD}${BLUE}║${RESET}"
  [[ -n "$remote" ]] && \
  printf "${BOLD}${BLUE}║${RESET}  📦 %-34s${BOLD}${BLUE}║${RESET}\n" "$remote"
  echo -e "${BOLD}${BLUE}╚══════════════════════════════════════╝${RESET}"
  echo ""
}

# ── Vérification de la compilation (Pré-Release) ─────────────────────────────
check_build() {
  title "Compilation et propagation de la version"
  
  if [[ -f "Cargo.toml" ]]; then
    info "Exécution de 'cargo build'..."
    if ! cargo build; then
      error "La compilation Rust a échoué."
      return 1
    fi
    success "Compilation Rust réussie."
    
  elif [[ -f "CMakeLists.txt" || -f "src/CMakeLists.txt" ]]; then
    info "Génération et compilation CMake..."
    if ! cmake -B build; then
      error "La configuration CMake a échoué."
      return 1
    fi
    
    local cores
    cores=$(nproc 2>/dev/null || echo 2)
    info "Compilation sur ${cores} cœurs..."
    if ! cmake --build build -j"${cores}"; then
      error "La compilation C++ a échoué."
      return 1
    fi
    success "Compilation C++ réussie."
    
  else
    warn "Aucun système de build (Cargo/CMake) trouvé à la racine. On continue."
  fi
  echo ""
  return 0
}

# ── Commit + Push ─────────────────────────────────────────────────────────────
commit_and_push() {
  local msg="${1:-}"
  local branch
  branch=$(git rev-parse --abbrev-ref HEAD)

  setup_gitignore
  untrack_ignored
  smart_add

  if git diff --cached --quiet; then
    warn "Rien à committer — synchronisation uniquement."
    git pull --rebase origin "$branch"
    git push origin "$branch"
    success "Dépôt à jour."
    return
  fi

  if [[ -z "$msg" ]]; then
    read -rp "$(echo -e "${YELLOW}? ${RESET}Message de commit : ")" msg
    [[ -z "$msg" ]] && msg="Update"
  fi

  git commit -m "$msg"
  success "Commit : \"$msg\""

  info "Pull --rebase depuis origin/${branch}..."
  git pull --rebase origin "$branch"

  info "Push vers origin/${branch}..."
  git push origin "$branch"

  success "Tout est pushé ✓"
}

# ── Actions du menu ───────────────────────────────────────────────────────────
do_status() {
  title "État du dépôt"
  local branch
  branch=$(git symbolic-ref --short HEAD 2>/dev/null || echo "HEAD détachée")
  local remote
  remote=$(git remote get-url origin 2>/dev/null || echo "(pas de remote)")
  echo -e "  ${BOLD}Branche :${RESET} ${GREEN}${branch}${RESET}"
  echo -e "  ${BOLD}Remote  :${RESET} ${remote}"
  echo ""
  git status -sb
}

do_tag() {
  title "Créer un tag"
  info "Tags existants :"
  git tag --sort=-version:refname | head -10 || echo "  (aucun)"
  echo ""
  local tag tmsg
  read -rp "$(echo -e "${YELLOW}? ${RESET}Nom du tag (ex: v1.2.3) : ")" tag
  [[ -z "$tag" ]] && { error "Nom vide — annulé."; return; }
  if git rev-parse "$tag" &>/dev/null; then
    error "Le tag '${tag}' existe déjà."; return
  fi
  read -rp "$(echo -e "${YELLOW}? ${RESET}Message du tag (vide = tag léger) : ")" tmsg
  if [[ -n "$tmsg" ]]; then git tag -a "$tag" -m "$tmsg"; else git tag "$tag"; fi
  success "Tag '${tag}' créé."
  if confirm "Pusher ce tag sur origin ?"; then
    git push origin "$tag"
    success "Tag '${tag}' pushé."
  fi
}

do_pull() {
  title "Pull"
  local branch
  branch=$(git symbolic-ref --short HEAD)
  echo -e "  1) pull (merge)\n  2) pull --rebase\n"
  local c
  read -rp "$(echo -e "${YELLOW}? ${RESET}Choix [2] : ")" c
  case "${c:-2}" in
    1) git pull origin "$branch" && success "Pull effectué." ;;
    *) git pull --rebase origin "$branch" && success "Pull --rebase effectué." ;;
  esac
}

do_branch() {
  title "Branches"
  echo -e "  1) Lister\n  2) Créer\n  3) Changer\n  4) Supprimer\n  5) Retour\n"
  local c nb sb db
  read -rp "$(echo -e "${YELLOW}? ${RESET}Choix : ")" c
  case "$c" in
    1) git branch -vva ;;
    2)
      read -rp "$(echo -e "${YELLOW}? ${RESET}Nom de la nouvelle branche : ")" nb
      [[ -z "$nb" ]] && { error "Nom vide."; return; }
      git checkout -b "$nb" && success "Branche '${nb}' créée."
      if confirm "Pusher et créer l'upstream ?"; then
        git push --set-upstream origin "$nb"
      fi ;;
    3)
      git branch -a
      read -rp "$(echo -e "${YELLOW}? ${RESET}Nom de la branche : ")" sb
      git checkout "$sb" && success "Basculé sur '${sb}'." ;;
    4)
      git branch -vva
      read -rp "$(echo -e "${YELLOW}? ${RESET}Branche à supprimer : ")" db
      confirm "Supprimer '${db}' localement ?" && git branch -d "$db"
      confirm "Supprimer '${db}' sur origin aussi ?" && git push origin --delete "$db" ;;
    *) return ;;
  esac
}

do_stash() {
  title "Stash"
  echo -e "  1) Sauvegarder\n  2) Lister\n  3) Restaurer (pop)\n  4) Retour\n"
  local c sm
  read -rp "$(echo -e "${YELLOW}? ${RESET}Choix : ")" c
  case "$c" in
    1)
      read -rp "$(echo -e "${YELLOW}? ${RESET}Message (optionnel) : ")" sm
      if [[ -n "$sm" ]]; then git stash push -m "$sm"; else git stash push; fi
      success "Stash sauvegardé." ;;
    2) git stash list ;;
    3) git stash pop && success "Stash restauré." ;;
    *) return ;;
  esac
}

do_log() {
  title "Historique"
  git log --oneline --graph --decorate --color --all | head -30
}

do_init() {
  title "Initialiser un dépôt"
  if git rev-parse --git-dir &>/dev/null 2>&1; then
    warn "Un dépôt Git existe déjà ici."; return
  fi
  git init && success "Dépôt initialisé."
  local rurl
  read -rp "$(echo -e "${YELLOW}? ${RESET}URL du remote origin (vide pour ignorer) : ")" rurl
  if [[ -n "$rurl" ]]; then
    git remote add origin "$rurl" && success "Remote 'origin' configuré."
  fi
}

do_full_reset() {
  title "⚠️  Reset total du dépôt (Clean Slate)"

  local branch remote confirm_word init_msg
  branch=$(git symbolic-ref --short HEAD 2>/dev/null || echo "master")
  remote=$(git remote get-url origin 2>/dev/null || echo "")

  echo -e "  ${RED}${BOLD}ATTENTION — Cette opération est IRRÉVERSIBLE.${RESET}"
  echo ""
  echo -e "  Ce qui va se passer :"
  echo -e "  ${GREEN}✔${RESET} Tes fichiers actuels sont conservés localement"
  echo -e "  ${RED}✖${RESET} Tout l'historique Git local est supprimé"
  [[ -n "$remote" ]] && \
  echo -e "  ${RED}✖${RESET} L'historique GitHub est écrasé (force push)"
  [[ -n "$remote" ]] && \
  echo -e "  ${RED}✖${RESET} TOUS les Tags distants seront supprimés"
  echo -e "  ${RED}✖${RESET} TOUTES les Releases GitHub seront supprimées (via 'gh')"
  echo -e "  ${RED}✖${RESET} TOUTES les Pull Requests ouvertes seront fermées (via 'gh')"
  echo -e "  ${GREEN}✔${RESET} Un seul nouveau commit initial sera créé"
  echo ""
  echo -e "  Branche : ${GREEN}${branch}${RESET}"
  [[ -n "$remote" ]] && echo -e "  Remote  : ${remote}"
  echo ""
  sep
  echo ""
  echo -e "${RED}Tape exactement${RESET} ${BOLD}RESET${RESET} ${RED}pour confirmer (ou Entrée pour annuler) :${RESET}"
  read -rp "  → " confirm_word
  if [[ "$confirm_word" != "RESET" ]]; then
    warn "Annulé."
    return
  fi

  read -rp "$(echo -e "${YELLOW}? ${RESET}Message du commit initial [\"Initial commit\"] : ")" init_msg
  init_msg="${init_msg:-Initial commit}"

  echo ""
  if [[ -n "$remote" ]]; then
    info "Nettoyage distant (GitHub) en cours..."
    
    # 1. Suppression de tous les tags distants
    info "Suppression des tags distants..."
    local remote_tags
    remote_tags=$(git ls-remote --tags origin 2>/dev/null | awk -F/ '{print $3}' | sed 's/\^{}//' | sort -u)
    if [[ -n "$remote_tags" ]]; then
      while IFS= read -r t; do
        [[ -n "$t" ]] && git push origin --delete "$t" 2>/dev/null || true
      done <<< "$remote_tags"
      success "Tags distants supprimés."
    else
      info "Aucun tag distant trouvé."
    fi

    # 2. Utilisation de GitHub CLI pour PRs et Releases
    if command -v gh &>/dev/null && gh auth status &>/dev/null; then
      info "Fermeture des Pull Requests (GitHub)..."
      local prs
      prs=$(gh pr list --state open --json number --jq '.[].number' 2>/dev/null)
      if [[ -n "$prs" ]]; then
        while IFS= read -r pr; do
          [[ -n "$pr" ]] && gh pr close "$pr" -m "Fermeture automatique (Reset total du dépôt)" 2>/dev/null || true
        done <<< "$prs"
        success "Pull Requests fermées."
      fi
      
      info "Suppression des Releases (GitHub)..."
      local releases
      releases=$(gh release list 2>/dev/null | awk '{print $1}')
      if [[ -n "$releases" ]]; then
        while IFS= read -r r; do
          [[ -n "$r" ]] && gh release delete "$r" --yes 2>/dev/null || true
        done <<< "$releases"
        success "Releases supprimées."
      fi
    else
      warn "CLI GitHub ('gh') non trouvée ou non connectée. Ignoré pour les PRs et Releases."
    fi
  fi

  # 3. Nettoyage local
  info "Suppression du dossier .git local..."
  rm -rf .git

  info "Réinitialisation du dépôt..."
  git init -b "$branch" 2>/dev/null || { git init && git checkout -b "$branch" 2>/dev/null || true; }

  info "Ajout de tous les fichiers..."
  setup_gitignore
  smart_add

  git commit -m "$init_msg"
  success "Nouveau commit initial créé : \"${init_msg}\""

  # 4. Push du dépôt tout neuf
  if [[ -n "$remote" ]]; then
    git remote add origin "$remote"
    info "Force push vers origin/${branch}..."
    git push --force origin "$branch"
    success "Dépôt remis à zéro sur GitHub ✓"
  else
    warn "Pas de remote configuré — reset local uniquement."
  fi

  echo ""
  success "✅ Reset total terminé. Le dépôt est vierge (historique, tags, releases effacés)."
}

do_purge_large_file() {
  title "Purge fichier trop gros (>100 MB)"

  echo -e "  ${YELLOW}Fichiers les plus lourds dans l'historique Git :${RESET}"
  echo ""
  git rev-list --objects --all 2>/dev/null \
    | git cat-file --batch-check='%(objecttype) %(objectname) %(objectsize) %(rest)' 2>/dev/null \
    | awk '/^blob/ { printf "%s\t%s\n", $3, $4 }' | sort -rn | head -10 \
    | awk '{ printf "  %8.2f MB  %s\n", $1/1024/1024, $2 }' || echo "  (impossible de lister)"
  echo ""
  sep
  echo ""

  local filepath confirm_word do_ignore ignore_pattern=""
  read -rp "$(echo -e "${YELLOW}? ${RESET}Chemin du fichier à purger (ex: assets/model.nnue) : ")" filepath
  [[ -z "$filepath" ]] && { error "Chemin vide — annulé."; return; }

  echo ""
  echo -e "  Fichier à supprimer : ${RED}${filepath}${RESET}"
  echo -e "  ${YELLOW}Il restera sur le disque, mais sera purgé de Git.${RESET}"
  echo ""

  read -rp "$(echo -e "${YELLOW}? ${RESET}Ajouter au .gitignore ? [O/n] ")" do_ignore
  if [[ ! "${do_ignore,,}" =~ ^n ]]; then
    local ext="${filepath##*.}"
    echo -e "  1) Ignorer ce fichier exact     : ${filepath}"
    [[ "$ext" != "$filepath" ]] && echo -e "  2) Ignorer toute l'extension    : *.${ext}"
    local ichoice
    read -rp "$(echo -e "${YELLOW}? ${RESET}Choix [1] : ")" ichoice
    case "${ichoice:-1}" in
      2) ignore_pattern="*.${ext}" ;;
      *) ignore_pattern="$filepath" ;;
    esac
  fi

  echo ""
  echo -e "${RED}Tape exactement${RESET} ${BOLD}PURGE${RESET} ${RED}pour confirmer (IRRÉVERSIBLE) :${RESET}"
  read -rp "  → " confirm_word
  [[ "$confirm_word" != "PURGE" ]] && { warn "Annulé."; return; }

  echo ""
  info "Réécriture de l'historique..."
  git filter-branch --force --index-filter "git rm --cached --ignore-unmatch '${filepath}'" --prune-empty --tag-name-filter cat -- --all

  info "Nettoyage..."
  git for-each-ref --format="delete %(refname)" refs/original 2>/dev/null | git update-ref --stdin || true
  git reflog expire --expire=now --all
  git gc --prune=now --aggressive
  success "Historique nettoyé."

  if [[ -n "$ignore_pattern" ]]; then
    if ! grep -qxF "$ignore_pattern" .gitignore 2>/dev/null; then
      printf '%s\n' "$ignore_pattern" >> .gitignore
      git add .gitignore
      git commit -m "chore: ignore ${ignore_pattern}"
      success "Ajouté au .gitignore : ${ignore_pattern}"
    fi
  fi

  local branch remote
  branch=$(git symbolic-ref --short HEAD)
  remote=$(git remote get-url origin 2>/dev/null || echo "")

  if [[ -n "$remote" ]]; then
    if confirm "Force push vers origin/${branch} ?"; then
      git push --force origin "$branch"
      git push --force --tags origin 2>/dev/null || true
      success "Force push effectué ✓"
    fi
  fi
  echo ""
  success "✅ Fichier purgé."
}

# ── Annuler la dernière release (Tag local + distant + GitHub Release) ───────
do_cancel_release() {
  title "Annuler / Supprimer la dernière Release ou Tag"

  local last_tag
  last_tag=$(git tag --sort=-version:refname | head -n 1)

  if [[ -z "$last_tag" ]]; then
    error "Aucun tag trouvé dans ce dépôt."
    return
  fi

  warn "Le dernier tag détecté est : ${BOLD}${last_tag}${RESET}"
  echo -e "  Cette opération va :"
  echo -e "  ${RED}✖${RESET} Supprimer le tag localement (${last_tag})"
  echo -e "  ${RED}✖${RESET} Supprimer le tag distant sur origin"
  echo -e "  ${RED}✖${RESET} Tenter d'effacer la Release sur GitHub (via la CLI 'gh' si dispo)"
  echo ""

  if ! confirm "Confirmer l'annulation complète de la release ${last_tag} ?"; then
    warn "Annulé."
    return
  fi

  echo ""
  info "Suppression du tag local..."
  git tag -d "$last_tag"

  local remote
  remote=$(git remote get-url origin 2>/dev/null || echo "")

  if [[ -n "$remote" ]]; then
    info "Suppression du tag distant sur origin..."
    if ! git push origin --delete "$last_tag"; then
      warn "Le tag distant n'a pas pu être supprimé (peut-être déjà inexistant)."
    fi

    # Nettoyage de la release sur GitHub via la CLI gh si installée et connectée
    if command -v gh &>/dev/null; then
      if gh auth status &>/dev/null; then
        info "GitHub CLI détectée. Suppression de la Release GitHub..."
        if gh release delete "$last_tag" --yes 2>/dev/null; then
          success "Release GitHub supprimée avec succès."
        else
          info "Aucune release GitHub correspondante trouvée (tag uniquement)."
        fi
      else
        warn "GitHub CLI (gh) trouvée mais non authentifiée. Impossible de supprimer la Release GitHub."
      fi
    else
      info "Note : Si une Release GitHub a été partiellement créée, installe 'gh' (GitHub CLI) pour permettre sa purge automatique."
    fi
  else
    warn "Pas de remote configuré — suppression locale uniquement."
  fi

  echo ""
  success "✅ Nettoyage terminé pour ${last_tag}."
}

# ── Release Universelle : Cargo.toml (Rust) ou CMakeLists.txt (C++) ──────────
do_release() {
  title "Release (commit + push + tag → GitHub Actions)"

  local branch
  branch=$(git rev-parse --abbrev-ref HEAD)

  local project_type="unknown"
  local version_file=""
  local current_version=""

  # Détection du type de projet
  if [[ -f "Cargo.toml" ]]; then
    project_type="rust"
    version_file="Cargo.toml"
    current_version=$(grep -m1 '^version\s*=' "$version_file" | sed 's/.*"\(.*\)".*/\1/')
  elif [[ -f "CMakeLists.txt" ]]; then
    project_type="cpp"
    version_file="CMakeLists.txt"
    current_version=$(grep -oP 'VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' "$version_file" | head -1)
  elif [[ -f "src/CMakeLists.txt" ]]; then
    project_type="cpp"
    version_file="src/CMakeLists.txt"
    current_version=$(grep -oP 'VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' "$version_file" | head -1)
  fi

  local new_version=""

  if [[ "$project_type" == "unknown" || -z "$current_version" ]]; then
    warn "Cargo.toml ou CMakeLists.txt introuvable ou version illisible."
    read -rp "$(echo -e "${YELLOW}? ${RESET}Saisir la version manuellement (ex: 1.2.3) ou [Entrée] pour annuler : ")" new_version
    [[ -z "$new_version" ]] && return
  else
    info "Projet détecté : ${BOLD}${project_type^^}${RESET} (fichier principal : ${version_file})"
    info "Version actuelle : ${BOLD}${current_version}${RESET}"

    local major minor patch
    IFS='.' read -r major minor patch <<< "$current_version"
    local next_patch="${major}.${minor}.$((patch + 1))"
    local next_minor="${major}.$((minor + 1)).0"
    local next_major="$((major + 1)).0.0"

    echo ""
    echo -e "  Nouvelle version :"
    echo -e "  ${BOLD}1${RESET}  Patch  ${GREEN}${next_patch}${RESET}"
    echo -e "  ${BOLD}2${RESET}  Minor  ${GREEN}${next_minor}${RESET}"
    echo -e "  ${BOLD}3${RESET}  Major  ${GREEN}${next_major}${RESET}"
    echo -e "  ${BOLD}4${RESET}  Saisir manuellement"
    echo ""

    local vchoice
    read -rp "$(echo -e "${YELLOW}? ${RESET}Choix [1] : ")" vchoice
    case "${vchoice:-1}" in
      2) new_version="$next_minor" ;;
      3) new_version="$next_major" ;;
      4)
        read -rp "$(echo -e "${YELLOW}? ${RESET}Version (ex: 1.2.3) : ")" new_version
        [[ -z "$new_version" ]] && { error "Version vide — annulé."; return; } ;;
      *) new_version="$next_patch" ;;
    esac
  fi

  local new_tag="v${new_version}"

  # Vérification si le tag existe déjà (avec proposition d'écrasement)
  if git rev-parse "$new_tag" &>/dev/null; then
    warn "Le tag '${new_tag}' existe déjà localement."
    
    if confirm "Voulez-vous le supprimer et l'écraser pour relancer la release ?"; then
      echo ""
      info "Nettoyage de l'ancien tag en cours..."
      
      # 1. Suppression locale
      git tag -d "$new_tag"
      
      # 2. Suppression distante (origin)
      local remote
      remote=$(git remote get-url origin 2>/dev/null || echo "")
      if [[ -n "$remote" ]]; then
        git push origin --delete "$new_tag" 2>/dev/null || warn "Le tag distant n'existait pas ou n'a pas pu être supprimé."
        
        # 3. Suppression de la Release GitHub (si gh CLI est présent)
        if command -v gh &>/dev/null && gh auth status &>/dev/null; then
          gh release delete "$new_tag" --yes 2>/dev/null || true
        fi
      fi
      success "Ancien tag '${new_tag}' purgé. Reprise de la release..."
      echo ""
    else
      error "Annulé."
      return
    fi
  fi

  echo ""
  if [[ -n "$current_version" ]]; then
    info "Mise à jour : ${BOLD}${current_version}${RESET} → ${BOLD}${GREEN}${new_version}${RESET}"
    echo ""
    
    # Remplacement de la version dans le fichier projet racine UNIQUEMENT
    if [[ "$project_type" == "rust" ]]; then
      sed -i "0,/^version = \"${current_version}\"/s/^version = \"${current_version}\"/version = \"${new_version}\"/" "$version_file"
      success "Cargo.toml mis à jour."
    elif [[ "$project_type" == "cpp" ]]; then
      sed -i "s/VERSION ${current_version}/VERSION ${new_version}/g" "$version_file"
      success "${version_file} mis à jour."
    fi
  fi

  # Lancer la vérification de compilation APRÈS la mise à jour pour que le build propage la version
  if ! check_build; then
    error "Release interrompue (échec de compilation). Corrigez les erreurs et relancez."
    return
  fi

  # ── Commit et Push ──
  setup_gitignore
  untrack_ignored
  smart_add

  local rel_msg
  read -rp "$(echo -e "${YELLOW}? ${RESET}Message de commit [\"chore: release ${new_version}\"] : ")" rel_msg
  rel_msg="${rel_msg:-chore: release ${new_version}}"
  git commit -m "$rel_msg"
  success "Commit : \"${rel_msg}\""

  info "Pull --rebase depuis origin/${branch}..."
  git pull --rebase origin "$branch"

  info "Push des commits vers origin/${branch}..."
  git push origin "$branch"
  success "Commits pushés."

  # ── Création et Push du Tag ──
  local tag_msg
  read -rp "$(echo -e "${YELLOW}? ${RESET}Description du tag [\"Release ${new_version}\"] : ")" tag_msg
  tag_msg="${tag_msg:-Release ${new_version}}"
  git tag -a "$new_tag" -m "$tag_msg"
  success "Tag annoté '${new_tag}' créé."

  info "Push du tag ${new_tag} → GitHub Actions..."
  git push origin "$new_tag"
  success "Tag '${new_tag}' pushé ✓"

  local remote repo
  remote=$(git remote get-url origin 2>/dev/null || echo "")
  echo ""
  if [[ -n "$remote" ]]; then
    repo=$(echo "$remote" | sed 's/.*github.com[:/]//' | sed 's/\.git$//')
    success "✅ Release ${new_tag} déclenchée → https://github.com/${repo}/actions"
  else
    success "✅ Release ${new_tag} terminée."
  fi
}

# ── Menu interactif ───────────────────────────────────────────────────────────
main_menu() {
  while true; do
    echo ""
    if git rev-parse --git-dir &>/dev/null 2>&1; then
      show_branch_banner
    else
      echo -e "${BOLD}${BLUE}╔══════════════════════════════════════╗${RESET}"
      echo -e "${BOLD}${BLUE}║        🚀  Assistant Git             ║${RESET}"
      echo -e "${BOLD}${BLUE}╚══════════════════════════════════════╝${RESET}"
      echo ""
    fi

    echo -e "  ${BOLD}1${RESET}  Commit + Push            ${CYAN}← équivalent à ./git.sh \"msg\"${RESET}"
    echo -e "  ${BOLD}2${RESET}  Release                  ${GREEN}← Auto-bump + Build + Push/Tag${RESET}"
    echo -e "  ${BOLD}3${RESET}  Créer & pusher un tag"
    echo -e "  ${BOLD}4${RESET}  Pull / Synchroniser"
    echo -e "  ${BOLD}5${RESET}  Branches"
    echo -e "  ${BOLD}6${RESET}  Stash"
    echo -e "  ${BOLD}7${RESET}  Historique (log)"
    echo -e "  ${BOLD}8${RESET}  Status"
    echo -e "  ${BOLD}9${RESET}  ${RED}Annuler la dernière release${RESET} ${RED}(efface tag local/distant + GH Release)${RESET}"
    echo -e "  ${BOLD}0${RESET}  Initialiser un nouveau dépôt"
    echo -e "  ${BOLD}r${RESET}  ${RED}Reset total${RESET} ${RED}(efface l'historique, garde les fichiers)${RESET}"
    echo -e "  ${BOLD}f${RESET}  ${YELLOW}Purger un fichier trop gros${RESET} ${YELLOW}(fichier >100MB rejeté par GitHub)${RESET}"
    echo -e "  ${BOLD}q${RESET}  Quitter"
    echo ""
    sep

    local choice
    read -rp "$(echo -e "${YELLOW}➜ ${RESET}Choix : ")" choice

    case "$choice" in
      1|2|3|4|5|6|7|8|9|r|R|f|F) require_git ;;
    esac

    case "$choice" in
      1) commit_and_push "" ;;
      2) do_release ;;
      3) do_tag ;;
      4) do_pull ;;
      5) do_branch ;;
      6) do_stash ;;
      7) do_log ;;
      8) do_status ;;
      9) do_cancel_release ;;
      0) do_init ;;
      r|R) do_full_reset ;;
      f|F) do_purge_large_file ;;
      q|Q) echo -e "\n${GREEN}À bientôt !${RESET}\n"; exit 0 ;;
      *) warn "Choix invalide." ;;
    esac
  done
}

# ── Point d'entrée ────────────────────────────────────────────────────────────
if [[ $# -gt 0 ]]; then
  require_git
  show_branch_banner
  commit_and_push "$*"
else
  main_menu
fi