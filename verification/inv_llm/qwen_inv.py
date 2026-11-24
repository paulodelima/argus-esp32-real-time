#!/usr/bin/env python3
"""
qwen_inv.py -- ciclo automatico "LLM propoe, ESBMC decide" para a invariante
do passo indutivo do ARGUS.

Cada volta:
  1. monta o prompt (codigo do firmware + invariante base + reforco atual +
     contraexemplo/erro da volta anterior) e chama o LLM local (Ollama);
  2. extrai do texto a funcao C  static int inv_extra(const argus_ctx_t *c);
  3. FILTRO: compila com gcc e roda execucoes aleatorias reais; se a
     invariante falha numa execucao real, a proposta e rejeitada;
  4. ESBMC caso base e passo indutivo (firmware abstraido com K slots);
  5. passo PASSA -> fim (prova); FALHA -> o contraexemplo volta ao passo 1.
A volta 0 roda sem reforco (inv_extra devolve 1), reproduzindo o ponto de
partida. Tudo fica registrado em verification/inv_llm/runs/<data-hora>/.

Claude pela API (mesmo prompt, sem contexto do projeto; credencial do `ant auth login`):
  python3 verification/inv_llm/qwen_inv.py --backend anthropic --model claude-opus-5-5
Executar na raiz do repositorio (Ollama rodando):
  python3 verification/inv_llm/qwen_inv.py --model qwen2.5-coder:7b
Teste sem LLM (respostas prontas, para conferir o encadeamento):
  python3 verification/inv_llm/qwen_inv.py --dry-run
"""
import argparse, csv, datetime, json, os, re, shutil, subprocess, sys, time
import urllib.error, urllib.request

ROOT = os.getcwd()
H = "verification/esbmc/argus_harness.c"
FW = "firmware_v2"
INV_DIR = "verification/inv_llm"
INV_H = os.path.join(INV_DIR, "inv_llm_current.h")

BASELINE = "static int inv_extra(const argus_ctx_t *c) {\n    (void)c;\n    return 1;\n}\n"

# Respostas prontas para --dry-run (nao sao do LLM; so testam o encadeamento).
DRY = [
"""```c
static int inv_extra(const argus_ctx_t *c) {
    int i, j;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        if (!c->tracks[i].active) continue;
        if (c->tracks[i].id < 1 || c->tracks[i].id > 0x3FFFFFFF) return 0;
        if (c->tracks[i].id == c->next_id) return 0;
        for (j = i + 1; j < ARGUS_MAX_TRACKS; j++)
            if (c->tracks[j].active && c->tracks[j].id == c->tracks[i].id) return 0;
    }
    return 1;
}
```""",
"""```c
static int inv_extra(const argus_ctx_t *c) {
    int i, j, dist;
    for (i = 0; i < ARGUS_MAX_TRACKS; i++) {
        if (!c->tracks[i].active) continue;
        if (c->tracks[i].id < 1 || c->tracks[i].id > 0x3FFFFFFF) return 0;
        dist = c->next_id - c->tracks[i].id;
        if (dist <= 0) dist += 0x3FFFFFFF;
        if (dist < 1 || dist > c->tracks[i].age * MAX_DETS) return 0;
        for (j = i + 1; j < ARGUS_MAX_TRACKS; j++)
            if (c->tracks[j].active && c->tracks[j].id == c->tracks[i].id) return 0;
    }
    return 1;
}
```""",
]

PROMPT = """Voce e um especialista em verificacao formal de software embarcado em C.

Firmware (argus_fsm.h):
```c
{h}
```

Firmware (argus_fsm.c):
```c
{c}
```

Contexto da prova: o harness executa um frame por volta: entrada realista
(ate MAX_DETS={max_dets} deteccoes por frame, x em [0,160), y fixo), chama
argus_update e depois o host drena a fila de eventos (argus_poll_event).
Estado inicial: argus_init(&c, 160, 120). next_id volta de 0x3FFFFFFF para 1.
Prova por inducao: caso base (o estado inicial satisfaz a invariante) e passo
(de QUALQUER estado que satisfaz a invariante, um frame a preserva e as
propriedades valem). A invariante e  inv_ok(c, prev) && inv_extra(c).

inv_ok atual (fixa, nao reescreva):
```c
{inv_ok}
```

inv_extra atual:
```c
{extra}
```

{feedback}

Tarefa: escreva uma NOVA versao de inv_extra que exclua o estado de partida
acima SE ele for inalcancavel a partir de argus_init, e que continue
verdadeira em TODA execucao real. Seja minimo. Regras:
- assinatura exata: static int inv_extra(const argus_ctx_t *c)
- devolve 1 se o estado e aceito, 0 caso contrario
- use apenas campos de c, macros ARGUS_* e MAX_DETS; lacos so ate ARGUS_MAX_TRACKS
- nada de printf, malloc, chamadas a outras funcoes ou variaveis globais
Responda com UM unico bloco ```c contendo apenas a funcao, e depois, em ate
3 frases, por que ela se preserva em um frame.
"""


def sh(cmd, timeout=None):
    t0 = time.time()
    try:
        p = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
        out = p.stdout + p.stderr
        rc = p.returncode
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or "") if isinstance(e.stdout, str) else ""
        out += "\n[TIMEOUT DO SCRIPT]"
        rc = -9
    return rc, out, time.time() - t0


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def write(path, txt):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(txt)


def inv_ok_text():
    s = read(H)
    m = re.search(r"static int inv_ok\(.*?\n}\n", s, re.S)
    if not m:
        return "(nao encontrada)"
    # O bloco INV_IDS e a resposta (a invariante ja provada); nao pode ir
    # no prompt, senao o LLM so copia. Fica fora em todas as rodadas.
    return re.sub(r"#ifdef INV_IDS\n.*?#endif\n", "", m.group(0), flags=re.S)


def anthropic_token():
    """Credencial do `ant auth login` (OAuth, renovada pelo proprio ant) ou,
    na falta dele, ANTHROPIC_API_KEY. O valor nunca e impresso nem gravado."""
    key = os.environ.get("ANTHROPIC_API_KEY")
    if key:
        return {"x-api-key": key}
    r = subprocess.run(["ant", "auth", "print-credentials", "--access-token"],
                       capture_output=True, text=True)
    tok = r.stdout.strip()
    if r.returncode != 0 or not tok:
        sys.exit("nao consegui a credencial: rode `ant auth login` (ou defina ANTHROPIC_API_KEY)")
    return {"Authorization": "Bearer " + tok}


def ask_anthropic(args, prompt, use_temp=True):
    body = {"model": args.model, "max_tokens": args.max_tokens,
            "messages": [{"role": "user", "content": prompt}]}
    if use_temp:
        body["temperature"] = args.temperature
    hdr = {"Content-Type": "application/json", "anthropic-version": args.anthropic_version}
    hdr.update(anthropic_token())
    req = urllib.request.Request(args.anthropic_url, data=json.dumps(body).encode(), headers=hdr)
    try:
        with urllib.request.urlopen(req, timeout=args.llm_timeout) as r:
            data = json.loads(r.read())
    except urllib.error.HTTPError as e:
        msg = e.read().decode(errors="replace")
        if use_temp and e.code == 400 and "temperature" in msg:
            print("  (modelo recusou temperature; repetindo sem ela)")
            return ask_anthropic(args, prompt, use_temp=False)
        sys.exit(f"API Anthropic: HTTP {e.code}: {msg[:500]}")
    return "".join(b.get("text", "") for b in data.get("content", []) if b.get("type") == "text")


def ask_llm(args, prompt, k):
    if args.dry_run:
        return DRY[min(k - 1, len(DRY) - 1)]
    if args.backend == "anthropic":
        return ask_anthropic(args, prompt)
    body = json.dumps({"model": args.model, "prompt": prompt, "stream": False,
                       "options": {"temperature": args.temperature, "num_ctx": 16384}}).encode()
    req = urllib.request.Request(args.ollama + "/api/generate", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=args.llm_timeout) as r:
        return json.loads(r.read())["response"]


def extract(resp):
    for m in re.finditer(r"```(?:c|C)?\s*\n(.*?)```", resp, re.S):
        if "inv_extra" in m.group(1):
            return m.group(1).strip() + "\n"
    m = re.search(r"static int inv_extra\s*\(.*?\n}\s*", resp, re.S)
    return m.group(0).strip() + "\n" if m else None


def rule_violations(prop):
    """Regras do prompt checadas no texto: a invariante nao pode ter efeito
    colateral nem chamar funcoes (o 7B chamou argus_poll_event dentro dela)."""
    body = re.sub(r"static int inv_extra\s*\([^)]*\)", "", prop)
    # comentarios nao contam: "/* distancia ciclica (no anel) */" nao e chamada
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    v = []
    calls = [f for f in re.findall(r"\b([A-Za-z_]\w*)\s*\(", body)
             if f not in ("if", "for", "while", "return", "sizeof")]
    if calls:
        v.append("chama funcoes (" + ", ".join(sorted(set(calls))) + ")")
    if re.search(r"\(\s*argus_ctx_t\s*\*\s*\)", body):
        v.append("remove o const de c (cast para argus_ctx_t *)")
    if re.search(r"c->[\w\[\]\.\->+% ()]*?(?<![=!<>])=(?!=)", body):
        v.append("escreve em campos de c")
    if re.search(r"\bwhile\b", body):
        v.append("usa while (so for ate ARGUS_MAX_TRACKS e permitido)")
    return v


def norm(code):
    return re.sub(r"\s+", "", code)


def verdict(log):
    if "VERIFICATION FAILED" in log: return "FALHA"
    if "VERIFICATION SUCCESSFUL" in log: return "PASSA"
    return "SEM_VEREDICTO"


def counterexample(log, limit=3500):
    lines = log.splitlines()
    out, seen_state = [], False
    for i, ln in enumerate(lines):
        if not seen_state and re.search(r"\bc = \{", ln):
            out.append("Estado de partida: " + ln.strip()[:1500]); seen_state = True
        if re.search(r"next_id|\]\.id |\]\.id=|\.active", ln) and len(out) < 25:
            out.append(ln.strip()[:220])
        if "Violated property" in ln:
            out.append("\n".join(x.strip() for x in lines[i:i + 4]))
    return "\n".join(out)[:limit]


def esbmc(args, kind, dirk, logpath):
    common = (f'esbmc {H} {dirk}/argus_fsm.c -I"{ROOT}/{dirk}" -I"{ROOT}/{INV_DIR}" '
              f'-DARGUS_V2 -DENV_E2 -DINV -DINV_LLM -DMAX_DETS={args.max_dets} -DFIX_Y ')
    if kind == "base":
        cmd = common + "-DSTEPS=0 -DNO_P3 -DNO_P4 -DNO_P5 --boolector --unwind 9"
    else:
        cmd = (common + "-DINDUCT_STEP -DSTEPS=1 --no-bounds-check --no-pointer-check "
               f"--no-align-check --boolector --unwind 9 --timeout {args.esbmc_min}m")
    rc, out, dt = sh(cmd, timeout=args.esbmc_min * 60 + 60)
    write(logpath, cmd + "\n\n" + out)
    return verdict(out), dt, out


def fuzz(args, rdir):
    exe = os.path.join(rdir, "fuzz_bin")
    res = []
    for md in sorted({1, args.max_dets, 2}):
        cmd = (f'gcc -w -DCHECK_WITH_GCC -DFUZZ -DENV_E2 -DARGUS_V2 -DINV -DINV_LLM -DFIX_Y '
               f'-DSTEPS=80 -DMAX_DETS={md} -I{FW} -I{INV_DIR} {H} {FW}/argus_fsm.c -o {exe}')
        rc, out, _ = sh(cmd, timeout=120)
        if rc != 0:
            return "ERRO_COMPILACAO", out[-2500:]
        fails, msg = 0, ""
        for _ in range(args.fuzz_runs):
            r, o, _ = sh(exe, timeout=30)
            if r != 0:
                fails += 1; msg = msg or o.strip()[-400:]
        res.append((md, fails))
        if fails:
            return "REJEITADA", (f"A proposta falhou em {fails} de {args.fuzz_runs} execucoes "
                                 f"reais com MAX_DETS={md} (a invariante nao vale na pratica). "
                                 f"Mensagem: {msg}")
    return "OK", "; ".join(f"MAX_DETS={m}: 0/{args.fuzz_runs}" for m, _ in res)


def ensure_abs(k):
    d = f"verification/abs/k{k}"
    if not os.path.exists(os.path.join(d, "argus_fsm.h")):
        os.makedirs(d, exist_ok=True)
        shutil.copy(f"{FW}/argus_fsm.c", d)
        h = read(f"{FW}/argus_fsm.h").replace("#define ARGUS_MAX_TRACKS    8",
                                              f"#define ARGUS_MAX_TRACKS    {k}")
        if f"ARGUS_MAX_TRACKS    {k}" not in h:
            sys.exit("nao consegui ajustar ARGUS_MAX_TRACKS no header")
        write(os.path.join(d, "argus_fsm.h"), h)
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="qwen2.5-coder:7b")
    ap.add_argument("--backend", choices=["ollama", "anthropic"], default="ollama",
                    help="ollama (Qwen local) ou anthropic (Claude pela API, sem contexto do projeto)")
    ap.add_argument("--ollama", default="http://localhost:11434")
    ap.add_argument("--anthropic-url", default="https://api.anthropic.com/v1/messages")
    ap.add_argument("--anthropic-version", default="2023-06-01")
    ap.add_argument("--max-tokens", type=int, default=4096)
    ap.add_argument("--rounds", type=int, default=6)
    ap.add_argument("--slots", type=int, default=2)
    ap.add_argument("--max-dets", type=int, default=1)
    ap.add_argument("--fuzz-runs", type=int, default=300)
    ap.add_argument("--esbmc-min", type=int, default=10)
    ap.add_argument("--temperature", type=float, default=0.2)
    ap.add_argument("--llm-timeout", type=int, default=900)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--skip-esbmc", action="store_true", help="so para testar o script")
    args = ap.parse_args()
    if not os.path.exists(H):
        sys.exit("rode na raiz do repositorio (nao achei " + H + ")")

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    run = os.path.join(INV_DIR, "runs", stamp)
    os.makedirs(run, exist_ok=True)
    dirk = ensure_abs(args.slots)
    rows, extra, feedback, last_cex, seen = [], BASELINE, "", "", set()
    meta = dict(vars(args)); meta["inicio"] = stamp
    write(os.path.join(run, "config.json"), json.dumps(meta, indent=2, ensure_ascii=False))
    print(f"registro em {run}  (modelo={'DRY-RUN' if args.dry_run else args.model}, slots={args.slots})")

    for k in range(0, args.rounds + 1):
        rdir = os.path.join(run, f"volta_{k}")
        os.makedirs(rdir, exist_ok=True)
        row = dict(volta=k, origem="base" if k == 0 else "llm", filtro="", base="", passo="",
                   tempo_llm_s="", tempo_passo_s="", violada="")
        if k > 0:
            prompt = PROMPT.format(h=read(f"{FW}/argus_fsm.h"), c=read(f"{FW}/argus_fsm.c"),
                                   max_dets=args.max_dets, inv_ok=inv_ok_text(), extra=extra,
                                   feedback=feedback)
            write(os.path.join(rdir, "prompt.txt"), prompt)
            t0 = time.time()
            try:
                resp = ask_llm(args, prompt, k)
            except Exception as e:
                sys.exit(f"falha ao chamar o LLM: {e}  (o Ollama esta rodando? modelo baixado?)")
            row["tempo_llm_s"] = round(time.time() - t0, 1)
            write(os.path.join(rdir, "resposta.txt"), resp)
            prop = extract(resp)
            if not prop:
                row["filtro"] = "SEM_CODIGO"
                feedback = "Sua resposta anterior nao trouxe um bloco ```c com inv_extra. Responda no formato pedido."
                rows.append(row); print(row); continue
            bad = rule_violations(prop)
            if bad:
                row["filtro"] = "REGRAS"
                write(os.path.join(rdir, "inv_llm_current.h"), prop)
                write(os.path.join(rdir, "filtro.txt"), "; ".join(bad))
                feedback = ("A proposta anterior foi REJEITADA por violar as regras: " + "; ".join(bad) +
                            ". A invariante deve ser uma condicao pura sobre os campos de c, sem efeito "
                            "colateral. Releia o contraexemplo:\n" + last_cex)
                rows.append(row); print(row); continue
            if norm(prop) == norm(extra) or norm(prop) in seen:
                row["filtro"] = "REPETIDA"
                write(os.path.join(rdir, "inv_llm_current.h"), prop)
                feedback = ("A proposta anterior REPETE uma versao ja testada e rejeitada. Ela nao exclui "
                            "o estado de partida do contraexemplo. Compare campo a campo esse estado "
                            "com o que uma execucao real a partir de argus_init poderia produzir.\n" + last_cex)
                rows.append(row); print(row); continue
            seen.add(norm(prop))
            candidate = prop
        else:
            candidate = BASELINE
        write(INV_H, candidate)
        write(os.path.join(rdir, "inv_llm_current.h"), candidate)

        st, info = fuzz(args, rdir)
        row["filtro"] = st
        write(os.path.join(rdir, "filtro.txt"), info)
        if st != "OK":
            feedback = (f"A proposta anterior foi REJEITADA no filtro ({st}).\n{info}\n"
                        "Proponha outra versao de inv_extra a partir da atual.")
            rows.append(row); print(row); continue

        if args.skip_esbmc:
            row["base"] = row["passo"] = "PULADO"
            rows.append(row); print(row); extra = candidate; continue

        vb, _, _ = esbmc(args, "base", dirk, os.path.join(rdir, "base.log"))
        row["base"] = vb
        if vb != "PASSA":
            feedback = (f"O CASO BASE falhou ({vb}): o estado inicial de argus_init nao satisfaz "
                        "a invariante. Ela esta forte demais. Corrija inv_extra.")
            rows.append(row); print(row); continue

        vp, dt, out = esbmc(args, "passo", dirk, os.path.join(rdir, "passo.log"))
        row["passo"], row["tempo_passo_s"] = vp, round(dt, 1)
        m = re.search(r"Violated property:\s*\n.*\n\s*(.*)", out)
        row["violada"] = m.group(1).strip() if m else ""
        extra = candidate
        rows.append(row); print(row)
        if vp == "PASSA":
            break
        if vp == "FALHA":
            cex = counterexample(out)
            write(os.path.join(rdir, "contraexemplo.txt"), cex)
            last_cex = cex
            feedback = ("O PASSO INDUTIVO falhou. Contraexemplo do ESBMC (estado de partida "
                        "arbitrario que satisfaz a invariante atual, e a propriedade violada):\n"
                        + cex)
        else:
            feedback = ("O passo indutivo nao terminou no tempo (sem veredicto). Se possivel, "
                        "proponha uma inv_extra que restrinja mais o estado de partida, sem "
                        "deixar de valer nas execucoes reais.")

    with open(os.path.join(run, "voltas.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader(); w.writerows(rows)
    final = rows[-1] if rows else {}
    resumo = [f"# Ciclo LLM + ESBMC ({stamp})", "",
              f"- modelo: {'DRY-RUN' if args.dry_run else args.model}; slots: {args.slots}; "
              f"MAX_DETS: {args.max_dets}; filtro: {args.fuzz_runs} execucoes de 80 frames",
              f"- resultado: {'PROVA (passo PASSA)' if final.get('passo') == 'PASSA' else 'sem prova'} "
              f"em {len(rows)} voltas (incluindo a volta 0, sem reforco)", "",
              "| volta | origem | filtro | base | passo | violada | t_LLM (s) | t_passo (s) |",
              "|---|---|---|---|---|---|---|---|"]
    for r in rows:
        resumo.append(f"| {r['volta']} | {r['origem']} | {r['filtro']} | {r['base']} | {r['passo']} | "
                      f"{r['violada']} | {r['tempo_llm_s']} | {r['tempo_passo_s']} |")
    write(os.path.join(run, "RESUMO.md"), "\n".join(resumo) + "\n")
    print("\n".join(resumo))


if __name__ == "__main__":
    main()
