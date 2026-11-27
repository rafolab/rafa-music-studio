// diagnostika LOOP – posledna doba
(() => {
  const out = []; const log = (t) => { out.push(t); box.textContent = out.join("\n"); console.log("DIAG " + t); };
  const box = document.createElement("pre");
  box.style.cssText = "position:fixed;left:8px;top:8px;z-index:99;background:rgba(0,0,0,.9);color:#7f7;font:13px monospace;padding:6px;margin:0";
  document.body.appendChild(box);
  let st = null, tr = [];
  Rafa.on("state", s => st = s); Rafa.on("tracks", t => tr = (t && t.tracks) || []);
  const wait = ms => new Promise(r => setTimeout(r, ms));
  const until = async (f, ms = 8000) => { const t0 = Date.now(); while (Date.now() - t0 < ms) { if (f()) return true; await wait(20); } return false; };
  (async () => {
    await until(() => st, 20000); await wait(5000);
    Rafa.send("newSong"); await wait(1500);
    Rafa.send("setMonitoring", { on: false }); Rafa.send("setMetronome", { on: false });
    Rafa.send("setCountIn", { bars: 2 }); Rafa.send("setTempo", { bpm: 120 }); await wait(300);
    Rafa.send("setLoopBars", { bars: 4 }); Rafa.send("setLoop", { on: true }); await wait(800);
    log("loop " + st.loop.start.toFixed(3) + " - " + st.loop.end.toFixed(3) + " bars " + st.loop.lengthBars);
    log("T_REC " + Date.now());
    Rafa.send("record");
    await until(() => tr.length && tr[0].clips && tr[0].clips.length, 30000);
    log("T_CLIP " + Date.now() + " clip " + JSON.stringify({s:tr[0].clips[0].start,l:tr[0].clips[0].length}));
    for (let k = 1; k < 2; ++k) {
      await wait(4 * 2000 + 700);
      const before = tr.length;
      Rafa.send("trackAdd"); await wait(400);
      log("T_REC" + (k + 1) + " " + Date.now()); Rafa.send("record");
      await until(() => tr.length > before && tr[tr.length - 1].clips && tr[tr.length - 1].clips.length, 60000);
      log("T_CLIP" + (k + 1) + " " + Date.now());
    }
    await wait(4 * 2000 * 3 + 500);
    log("T_STOP " + Date.now()); Rafa.send("stop");
    await wait(500); log("DONE");
  })();
})();
