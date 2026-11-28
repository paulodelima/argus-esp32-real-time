# Revisao sistematica ARGUS: strings S1-S5b no OpenAlex (titulo+resumo, desde 2010)
import json,urllib.request,urllib.parse,sys,time
S={
"S1":'("bounded model checking" OR "k-induction") AND (firmware OR "embedded software" OR microcontroller OR "embedded C")',
"S2":'("bounded model checking" OR "model checking") AND ("timing constraint" OR "timing constraints" OR deadline OR "execution time") AND (embedded OR firmware)',
"S3":'("large language model" OR "large language models" OR LLM OR LLMs) AND ("loop invariant" OR "loop invariants" OR "inductive invariant" OR "inductive invariants" OR "invariant generation")',
"S4":'("model checking" OR "formal verification") AND ("finite state machine" OR "event-driven") AND ("cyber-physical" OR embedded OR firmware)',
"S5b":'("parameterized verification" OR "parameterized model checking" OR "symmetry reduction" OR "counter abstraction" OR "cutoff") AND ("model checking" OR "software verification")',
"S5":'("parameterized verification" OR "parameterized model checking" OR "symmetry reduction" OR "counter abstraction") AND (software OR "C program" OR "C programs" OR embedded OR protocol)',
}
out={}
for k,q in S.items():
  u="https://api.openalex.org/works?"+urllib.parse.urlencode({"filter":"title_and_abstract.search:"+q+",from_publication_date:2010-01-01","per-page":100,"sort":"cited_by_count:desc","mailto":"anon@example.org"})
  for t in range(5):
    try: d=json.load(urllib.request.urlopen(u,timeout=60));break
    except Exception as e: print(k,e,file=sys.stderr);time.sleep(5)
  def src(w):
    pl=w.get("primary_location") or {}; s=pl.get("source") or {}; return s.get("display_name")
  out[k]={"query":q,"count":d["meta"]["count"],"top":[{"t":w["title"],"y":w["publication_year"],"doi":w.get("doi"),"c":w["cited_by_count"],"v":src(w),"a":[x["author"]["display_name"] for x in w["authorships"][:5]]} for w in d["results"]]}
  print(k,d["meta"]["count"]); time.sleep(2)
json.dump(out,open("openalex_S1-S5.json","w"),ensure_ascii=False,indent=1)
