#pragma once

// Web UI served at GET / — mobile-first control panel for Android/iOS/any browser.
// Stored as a raw string literal in flash (const on R4 = flash, not RAM).
// ~4KB — well within the R4's 256KB flash budget.

const char HTML_PAGE[] = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>Mic Slider</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#1a1a1a;color:#e8e8e8;font-family:system-ui,sans-serif;padding:12px;max-width:480px;margin:auto}
h2{text-align:center;font-size:1rem;color:#777;margin-bottom:10px;letter-spacing:.05em;text-transform:uppercase}
.tabs{display:flex;gap:4px;margin-bottom:10px}
.tab{flex:1;padding:10px 0;background:#242424;border:none;color:#777;border-radius:6px;font-size:1rem;cursor:pointer;touch-action:manipulation}
.tab.on{background:#0a7;color:#fff;font-weight:bold}
.card{background:#242424;border-radius:10px;padding:12px;margin-bottom:10px}
.lbl{font-size:.72rem;color:#555;text-transform:uppercase;letter-spacing:.06em;margin-bottom:6px}
.track{background:#333;border-radius:4px;height:10px;overflow:hidden;margin-bottom:4px}
.fill{height:100%;background:#0a7;width:0;transition:width .25s}
.nums{font-size:.8rem;color:#555;text-align:right}
.conn{font-size:.85rem;margin-top:6px;color:#0a7}
.conn.bad{color:#e44}
.jog{display:flex;gap:10px;margin-bottom:8px}
.jog-btn{flex:1;padding:26px 0;border:none;background:#1a3560;color:#eee;border-radius:8px;font-size:1.15rem;cursor:pointer;touch-action:manipulation;user-select:none;-webkit-user-select:none}
.jog-btn:active{filter:brightness(1.4)}
.stop{width:100%;padding:14px;border:none;background:#5a1515;color:#fff;border-radius:8px;font-size:1rem;cursor:pointer;display:none;touch-action:manipulation}
.step-row{display:flex;gap:6px}
.step-btn{flex:1;padding:11px 0;border:none;background:#2a2a2a;color:#777;border-radius:6px;font-size:.9rem;cursor:pointer;touch-action:manipulation}
.step-btn.on{background:#383838;color:#fff;font-weight:bold}
.spd-row{display:flex;align-items:center;gap:10px}
.spd-row input[type=range]{flex:1;accent-color:#0a7}
.spd-lbl{font-size:.85rem;color:#777;min-width:62px;text-align:right}
.home-btn{width:100%;padding:14px;border:none;background:#2a2a2a;color:#ccc;border-radius:8px;font-size:1rem;cursor:pointer;touch-action:manipulation}
.home-btn:active{background:#383838}
</style>
</head>
<body>
<h2>Mic Slider</h2>
<div class="tabs" id="tabs"></div>
<div class="card">
  <div class="lbl">Position</div>
  <div class="track"><div class="fill" id="fill"></div></div>
  <div class="nums"><span id="posN">0</span> / <span id="maxN">20000</span></div>
  <div class="conn" id="conn">Connecting...</div>
</div>
<div class="card">
  <div class="lbl">Jog</div>
  <div class="jog">
    <button class="jog-btn" id="lBtn">&#9664;&#9664; LEFT</button>
    <button class="jog-btn" id="rBtn">RIGHT &#9654;&#9654;</button>
  </div>
  <button class="stop" id="stopBtn" onclick="doStop()">&#9632; STOP</button>
</div>
<div class="card">
  <div class="lbl">Step Size</div>
  <div class="step-row">
    <button class="step-btn" data-v="800"  onclick="setStep(this)">Fine</button>
    <button class="step-btn on" data-v="2000" onclick="setStep(this)">Med</button>
    <button class="step-btn" data-v="4000" onclick="setStep(this)">Coarse</button>
  </div>
</div>
<div class="card">
  <div class="lbl">Speed</div>
  <div class="spd-row">
    <input type="range" min="100" max="3000" value="2000" id="spd"
      oninput="document.getElementById('spdLbl').textContent=this.value+' s/s'"
      onchange="setSpeed(this.value)">
    <span class="spd-lbl" id="spdLbl">2000 s/s</span>
  </div>
</div>
<div class="card">
  <button class="home-btn" onclick="doHome()">&#8962; Home</button>
</div>
<script>
var sl=1, steps=2000, poll=null, hold=null, isHold=false;
var host=location.host;

// Build slider tabs
var tabsEl=document.getElementById('tabs');
for(var s=1;s<=6;s++){
  (function(s){
    var b=document.createElement('button');
    b.className='tab'+(s===1?' on':'');
    b.textContent=s;
    b.onclick=function(){
      sl=s;
      tabsEl.querySelectorAll('.tab').forEach(function(t,i){t.classList.toggle('on',i+1===s);});
      fetchStatus();
    };
    tabsEl.appendChild(b);
  })(s);
}

function apiUrl(path){
  return 'http://'+host+path+(path.indexOf('?')>=0?'&':'?')+'slider='+sl;
}
function req(path){
  return fetch(apiUrl(path)).then(function(r){return r.json();}).then(updateUI).catch(showErr);
}
function showErr(){
  var c=document.getElementById('conn');
  c.className='conn bad'; c.textContent='Offline';
}

function updateUI(s){
  var pct=s.maxSteps>0?(s.pos/s.maxSteps*100):0;
  document.getElementById('fill').style.width=pct+'%';
  document.getElementById('posN').textContent=s.pos;
  document.getElementById('maxN').textContent=s.maxSteps;
  document.getElementById('spd').value=s.speed;
  document.getElementById('spdLbl').textContent=s.speed+' s/s';
  var c=document.getElementById('conn');
  c.className='conn';
  c.textContent=s.moving?(s.homing?'Homing...':'Moving'):'Connected \u2713';
  document.getElementById('stopBtn').style.display=s.moving?'block':'none';
  if(s.moving&&!poll) poll=setInterval(fetchStatus,350);
  if(!s.moving&&poll){clearInterval(poll);poll=null;}
}

function fetchStatus(){ req('/status'); }
function doStop(){ req('/jog/stop'); }
function doHome(){ req('/home'); }

function setStep(btn){
  steps=parseInt(btn.dataset.v);
  document.querySelectorAll('.step-btn').forEach(function(b){b.classList.remove('on');});
  btn.classList.add('on');
}

function setSpeed(v){ req('/speed?val='+v); }

// Jog: tap = step move, hold (300ms) = continuous jog
function jogDown(dir){
  isHold=false;
  hold=setTimeout(function(){ isHold=true; req('/jog/start?dir='+dir); }, 300);
}
function jogUp(dir){
  clearTimeout(hold);
  if(!isHold) req('/move?dir='+dir+'&steps='+steps);
  else req('/jog/stop');
}

function wireBtn(btn, dir){
  btn.addEventListener('mousedown', function(){ jogDown(dir); });
  btn.addEventListener('mouseup',   function(){ jogUp(dir); });
  btn.addEventListener('mouseleave',function(){ if(isHold){ isHold=false; req('/jog/stop'); } });
  btn.addEventListener('touchstart',function(e){ e.preventDefault(); jogDown(dir); },{passive:false});
  btn.addEventListener('touchend',  function(e){ e.preventDefault(); jogUp(dir);   },{passive:false});
}
wireBtn(document.getElementById('lBtn'),'left');
wireBtn(document.getElementById('rBtn'),'right');

fetchStatus();
setInterval(fetchStatus, 5000);
</script>
</body>
</html>
)rawhtml";
