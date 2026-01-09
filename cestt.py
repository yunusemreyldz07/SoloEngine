#!/usr/bin/env python3
import os, sys, time, datetime, argparse, platform, shutil, glob, subprocess, shlex, threading, queue, traceback, signal
import psutil
import chess, chess.pgn

LOG_DIR="logs"; PGN_DIR="pgn"
os.makedirs(LOG_DIR, exist_ok=True); os.makedirs(PGN_DIR, exist_ok=True)
ASAN_SIGS=("ASAN:","AddressSanitizer","ERROR: AddressSanitizer","heap-use-after-free","heap-buffer-overflow","stack-buffer-overflow","double-free","SEGV","SIGABRT")

def log(m,err=False):
    t=datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    s=f"[{t}] {m}"
    print(s, file=sys.stderr if err else sys.stdout, flush=True)
    with open(os.path.join(LOG_DIR,"stress.log"),"a",encoding="utf-8") as f: f.write(s+"\n")

def posix(): return os.name=="posix"
def win(): return platform.system()=="Windows"

def enable_core():
    if not posix(): return
    try:
        import resource; resource.setrlimit(resource.RLIMIT_CORE,(resource.RLIM_INFINITY,resource.RLIM_INFINITY))
        log("core dumps enabled")
    except Exception as e: log(f"core dumps not enabled: {e}")

def find_debugger():
    sysname=platform.system()
    if sysname in ("Darwin","Linux") and shutil.which("lldb"):
        return ("lldb","lldb -b -o 'bt all' -o 'quit' -- {exe} -c {core}")
    if sysname=="Windows" and shutil.which("cdb"):
        return ("cdb",'cdb -z "{core}" -c "!analyze -v; ~* kb; q"')
    return (None,None)

def discover_core(pid_hint=None):
    s=platform.system(); found=[]
    try:
        if s=="Darwin":
            found+=sorted(glob.glob("/cores/core.*"), key=os.path.getmtime, reverse=True)
        elif s=="Linux":
            found+=sorted(glob.glob("core*"), key=os.path.getmtime, reverse=True)
            found+=sorted(glob.glob("**/core*",recursive=True), key=os.path.getmtime, reverse=True)
            p="/var/lib/apport/coredump"
            if os.path.isdir(p): found+=sorted(glob.glob(os.path.join(p,"CoreDump*")), key=os.path.getmtime, reverse=True)
        else:
            la=os.environ.get("LOCALAPPDATA","")
            d=os.path.join(la,"CrashDumps") if la else ""
            if d and os.path.isdir(d): found+=sorted(glob.glob(os.path.join(d,"*.dmp")), key=os.path.getmtime, reverse=True)
            found+=sorted(glob.glob("*.dmp"), key=os.path.getmtime, reverse=True)
    except Exception as e: log(f"core discovery error: {e}")
    if pid_hint is not None:
        fl=[p for p in found if str(pid_hint) in os.path.basename(p)]
        return fl if fl else found
    return found

def run_debugger(engine_path, core_path, out_file):
    dbg,pat=find_debugger()
    if not dbg:
        with open(out_file,"a",encoding="utf-8") as f: f.write("\n=== DEBUGGER NOT FOUND ===\n")
        return
    cmd=pat.format(exe=shlex.quote(engine_path), core=core_path)
    try:
        log(f"debugger: {cmd}")
        r=subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
        with open(out_file,"a",encoding="utf-8") as f:
            f.write(f"\n=== {dbg.upper()} BACKTRACE ===\n")
            f.write(r.stdout.decode(errors="ignore"))
    except Exception as e:
        with open(out_file,"a",encoding="utf-8") as f: f.write(f"\n[DEBUGGER ERROR] {e}\n")

def dump_crash(ctx, stderr_blob=b"", pid_hint=None, note=None):
    ts=int(time.time()); fp=os.path.join(LOG_DIR,f"crash_{ts}.log")
    st=stderr_blob.decode(errors="ignore") if isinstance(stderr_blob,(bytes,bytearray)) else (stderr_blob or "")
    with open(fp,"w",encoding="utf-8") as f:
        f.write("=== CRASH DETECTED ===\n")
        for k,v in ctx.items(): f.write(f"{k}: {v}\n")
        if note: f.write(f"Note: {note}\n")
        f.write("\n=== STDERR / ASAN ===\n"); f.write(st+"\n")
    cores=discover_core(pid_hint)
    if cores: run_debugger(ctx.get("ENGINE_PATH","<unknown>"), cores[0], fp)
    log(f"crash -> {fp}")

class UCIEngine:
    def __init__(self, cmd, tag, env=None):
        self.proc=psutil.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, text=True, bufsize=1, universal_newlines=True)
        self.tag=tag
        self.q=queue.Queue()
        self.asan_q=queue.Queue()
        self.buf=[]
        self.rd=threading.Thread(target=self._reader,daemon=True); self.rd.start()
        self._send("uci"); self._wait("uciok"); self._send("isready"); self._wait("readyok"); self._send("ucinewgame"); self._send("isready"); self._wait("readyok")
    def _reader(self):
        with open(os.path.join(LOG_DIR,f"engine_{self.tag}.log"),"a",encoding="utf-8") as lf, \
             open(os.path.join(LOG_DIR,f"stderr_{self.tag}.log"),"a",encoding="utf-8") as ef:
            for line in self.proc.stdout:
                s=line.rstrip("\n")
                lf.write(s+"\n")
                if any(k in s for k in ASAN_SIGS): 
                    self.asan_q.put(s); ef.write(s+"\n")
                self.q.put(s)
    def _send(self, s): 
        try: self.proc.stdin.write(s+"\n"); self.proc.stdin.flush()
        except Exception: pass
    def _wait(self, token, timeout=10):
        t=time.time()
        while time.time()-t<timeout:
            try:
                s=self.q.get(timeout=0.05)
                if token in s: return True
            except queue.Empty: pass
        return False
    def newgame(self): self._send("ucinewgame"); self._send("isready"); self._wait("readyok")
    def go_bestmove(self, fen, depth):
        self._send(f"position fen {fen}")
        self._send(f"go depth {depth}")
        best=None; t=time.time()
        while True:
            try:
                s=self.q.get(timeout=0.5)
                if s.startswith("bestmove"):
                    parts=s.split()
                    best=parts[1] if len(parts)>1 else None
                    break
            except queue.Empty:
                if time.time()-t>30: break
        return best
    def kill(self):
        try:
            self._send("quit")
            self.proc.terminate()
        except Exception: pass

def worker(wid, engine_path, depth, max_moves, mem_limit_mb, spin_sec, games_limit):
    try:
        env=os.environ.copy()
        if posix(): env["ASAN_OPTIONS"]=env.get("ASAN_OPTIONS","detect_leaks=1:halt_on_error=1:abort_on_error=1")
        tag=f"w{wid}_{int(time.time())}"
        eng=UCIEngine([engine_path], tag, env=env)
        p=psutil.Process(eng.proc.pid)
        game_count=0
        while games_limit==0 or game_count<games_limit:
            game_count+=1
            board=chess.Board()
            game=chess.pgn.Game(); node=game
            last_move_time=time.time()
            for mv in range(1, max_moves+1):
                if not eng.asan_q.empty():
                    buf=[]
                    while not eng.asan_q.empty(): buf.append(eng.asan_q.get())
                    ctx={"ENGINE_PATH":engine_path,"WORKER":wid,"GAME":game_count,"MOVE":mv,"PID":eng.proc.pid,"FEN":board.fen()}
                    dump_crash(ctx,"\n".join(buf), pid_hint=eng.proc.pid, note="ASAN signal")
                    break
                rss=p.memory_info().rss/(1024*1024)
                try:
                    thsum=sum([t.system_time+t.user_time for t in p.threads()])
                except (psutil.AccessDenied, PermissionError):
                    thsum=0  # Skip thread monitoring on macOS without permissions
                log(f"[w{wid}] g{game_count} m{mv} rss {rss:.1f}MB thr_cpu {thsum:.2f}")
                if rss>mem_limit_mb:
                    ctx={"ENGINE_PATH":engine_path,"WORKER":wid,"GAME":game_count,"MOVE":mv,"PID":eng.proc.pid,"FEN":board.fen()}
                    dump_crash(ctx,b"MEMORY LIMIT", pid_hint=eng.proc.pid, note="memory limit")
                    break
                bm=eng.go_bestmove(board.fen(), depth)
                if not bm: 
                    if time.time()-last_move_time>spin_sec:
                        ctx={"ENGINE_PATH":engine_path,"WORKER":wid,"GAME":game_count,"MOVE":mv,"PID":eng.proc.pid,"FEN":board.fen()}
                        dump_crash(ctx,b"NO BESTMOVE/SPIN", pid_hint=eng.proc.pid, note="possible spin")
                    break
                try:
                    move=chess.Move.from_uci(bm)
                    if move not in board.legal_moves: break
                    board.push(move)
                    node=node.add_variation(move)
                    last_move_time=time.time()
                    if board.is_game_over(): break
                except Exception as e:
                    ctx={"ENGINE_PATH":engine_path,"WORKER":wid,"GAME":game_count,"MOVE":mv,"PID":eng.proc.pid,"FEN":board.fen()}
                    dump_crash(ctx, str(e), pid_hint=eng.proc.pid, note="illegal move parse")
                    break
            ts=datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            fn=os.path.join(PGN_DIR,f"w{wid}_g{game_count}_{ts}.pgn")
            with open(fn,"w",encoding="utf-8") as f: print(game, file=f)
        eng.kill()
    except Exception as e:
        log(f"[w{wid}] EXC {e}\n{traceback.format_exc()}", err=True)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--engine", required=True)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--games", type=int, default=0)
    ap.add_argument("--depth", type=int, default=12)
    ap.add_argument("--max-moves", type=int, default=100)
    ap.add_argument("--mem-limit-mb", type=float, default=2048.0)
    ap.add_argument("--spin-sec", type=float, default=10.0)
    args=ap.parse_args()

    if not os.path.isfile(args.engine): 
        log("engine not found", err=True); sys.exit(1)
    if posix(): enable_core()

    procs=[]
    for w in range(args.workers):
        p=threading.Thread(target=worker, args=(w,args.engine,args.depth,args.max_moves,args.mem_limit_mb,args.spin_sec,args.games), daemon=True)
        p.start(); procs.append(p)
    log("stress started")
    try:
        while any(t.is_alive() for t in procs): time.sleep(0.5)
    except KeyboardInterrupt:
        log("interrupt, exiting")
    log("done")

if __name__=="__main__": main()
