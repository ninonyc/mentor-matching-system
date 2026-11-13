const $ = s => document.querySelector(s);
const escape = x => String(x ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let state = {role:'mentee',token:null,demo:false,user:null,rounds:[],round:null,people:[],order:[],saved:false,run:null,runs:[],ratings:[],assignmentError:null};
let dragId = null, noticeTimer;
const demoPeople = [{user_id:10,name:'Alex Rivera',role:'mentor'},{user_id:20,name:'Morgan Chen',role:'mentor'},{user_id:30,name:'Sam Taylor',role:'mentor'},{user_id:40,name:'Jordan Ellis',role:'mentee'},{user_id:50,name:'Casey Brooks',role:'mentee'}];
// Static development servers use the separate loopback API; deployed pages
// use their own HTTPS origin through the reverse proxy.
const apiBase=(['127.0.0.1','localhost'].includes(location.hostname)&&['5173','4173'].includes(location.port))?'http://127.0.0.1:8080/api':'/api';
async function api(path,method='GET',body){const res=await fetch(apiBase+path,{method,headers:{...(state.token?{Authorization:'Bearer '+state.token}:{}),...(body?{'Content-Type':'application/json'}:{})},body:body?JSON.stringify(body):undefined});const data=await res.json();if(!res.ok){if(res.status===401&&state.token){logout(false);throw Error('Your session expired. Please sign in again.');}throw Object.assign(Error(data.error||'Request failed'),{status:res.status});}return data;}
function notify(message){$('#notice').textContent=message;clearTimeout(noticeTimer);noticeTimer=setTimeout(()=>$('#notice').textContent='',6000);}
function openLogin(role){state.role=role;$('#loginTitle').textContent=role==='mentor'?'Guide the next ascent.':role==='mentee'?'Begin your next chapter.':'A view of the whole journey.';$('#loginError').textContent='';$('#login').showModal();}
document.querySelectorAll('.entrance').forEach(b=>b.onclick=()=>openLogin(b.dataset.role));$('#adminLink').onclick=()=>openLogin('admin');function closeLogin(){ $('#login').close();$('#loginForm').reset();$('#loginError').textContent=''; }
$('#login .close').onclick=closeLogin;$('#backFromLogin').onclick=closeLogin;$('#login').addEventListener('click',e=>{if(e.target===$('#login'))closeLogin();});
$('#login').addEventListener('cancel',e=>{e.preventDefault();closeLogin();});
$('#loginForm').onsubmit=async e=>{e.preventDefault();$('#signIn').disabled=true;$('#loginError').textContent='';try{const form=new FormData(e.target);const login=await api('/auth/login','POST',{email:form.get('email'),password:form.get('password')});state.token=login.token;state.user=await api('/me');if(state.role==='admin'&&!state.user.is_admin){await api('/auth/logout','POST');state.token=null;throw Error('This account does not have administrator access.');}state.role=state.user.is_admin?'admin':state.role;state.demo=false;await enter();$('#login').close();e.target.reset();}catch(error){$('#loginError').textContent=error.message==='Failed to fetch'?'Cannot reach the API. Start the backend with the allowed frontend origin.':error.message;}finally{$('#signIn').disabled=false;}};
$('.demo').onclick=async()=>{state.demo=true;state.token=null;state.user={name:state.role==='admin'?'Administrator':state.role==='mentor'?'Alex Rivera':'Jordan Ellis',id:state.role==='mentor'?10:40,is_admin:state.role==='admin'};await enter();$('#login').close();$('#loginForm').reset();};
async function enter(){state.rounds=state.demo?[{id:1,name:'Autumn mentoring cohort',state:'frozen'}]:(await api('/rounds')).rounds;state.round=state.rounds[0]?.id??null;document.body.className='logged-in role-'+state.role;$('#welcome').hidden=true;$('#workspace').hidden=false;$('#topActions').innerHTML=`<span>${escape(state.user.name)}</span><button id="logout">Sign out ↗</button>`;$('#logout').onclick=()=>logout();await loadRound();}
async function logout(revoke=true){if(revoke&&state.token){try{await api('/auth/logout','POST');}catch{}}state={role:'mentee',token:null,demo:false,user:null,rounds:[],round:null,people:[],order:[],saved:false,run:null,runs:[],ratings:[],assignmentError:null};document.body.className='';$('#welcome').hidden=false;$('#workspace').hidden=true;$('#feedbackPanel').hidden=true;$('#topActions').innerHTML='';positionClimbers();}
async function loadRound(){state.run=null;state.runs=[];state.ratings=[];state.assignmentError=null;state.saved=false;state.order=[];if(!state.round){render();return;}state.people=state.demo?demoPeople:(await api(`/rounds/${state.round}/participants`)).participants;if(state.role!=='admin'){const own=state.people.find(x=>x.user_id===state.user.id);if(!own){throw Error('Your account is not a participant in this round.');}state.role=own.role;document.body.className='logged-in role-'+state.role;let preferences=null;if(state.demo){preferences={targets:state.people.filter(p=>p.role!==state.role).map(p=>p.user_id)};}else{try{preferences=await api(`/rounds/${state.round}/preferences`);}catch(e){if(e.status!==404)throw e;}}state.saved=!!preferences&&!state.demo;state.order=preferences?.targets??state.people.filter(p=>p.role!==state.role).map(p=>p.user_id);try{state.run=state.demo?null:await api(`/rounds/${state.round}/my-assignment`);}catch(e){if(e.status!==404)state.assignmentError=e.message;}}render();if(state.role==='admin')await overview();}
function name(id){return state.people.find(p=>p.user_id===id)?.name??'Participant '+id;}
function render(){const admin=state.role==='admin';$('#feedbackPanel').hidden=!admin||!state.round;if(admin&&state.round)renderFeedback();$('#workspace').innerHTML=`<button type="button" id="exitWorkspace" class="workspace-exit" aria-label="Exit workspace and return to mountain">Back to mountain <span aria-hidden="true">×</span></button><div class="workspace-head"><div><p class="eyebrow">${admin?'THE SUMMIT VIEW':'YOUR '+escape(state.role).toUpperCase()+' WORKSPACE'}</p><h2>${admin?'Bring the right people together.':'Build your preference list.'}</h2></div><select id="round" aria-label="Matching round">${state.rounds.map(r=>`<option value="${r.id}" ${r.id===state.round?'selected':''}>${escape(r.name)}</option>`).join('')}</select></div>${state.demo?'<div class="demo-banner">Demo workspace · Sample data only. Changes are not saved to the server.</div>':''}${!state.round?'<p class="subtext">No mentoring rounds are available for this account yet. Your administrator manages enrollment.</p>':admin?`<p class="subtext">Review each side’s preferences before assigning mentoring partners.</p><div class="columns"><section><h3>Mentors <span class="badge">${state.people.filter(p=>p.role==='mentor').length}</span></h3><div id="mentors"></div></section><section><h3>Mentees <span class="badge">${state.people.filter(p=>p.role==='mentee').length}</span></h3><div id="mentees"></div></section></div><p id="readiness" class="subtext"></p><div class="actions"><button class="primary" id="assign">Assign mentoring partners ↗</button><button class="secondary" id="refresh">Refresh</button></div><div id="matchResult"></div>`:`<p class="subtext">Rank every ${state.role==='mentor'?'mentee':'mentor'} from your first choice to your last. Drag to reorder, or use the arrows.</p><ol class="rank-list">${state.order.map((id,i)=>`<li class="rank-item" draggable="true" data-id="${id}"><strong>${String(i+1).padStart(2,'0')}</strong><div class="identity">${escape(name(id))}<small>${state.role==='mentor'?'Mentee':'Mentor'}</small></div><div class="arrows"><button data-move="${i},-1" aria-label="Move ${escape(name(id))} up" ${i===0?'disabled':''}>↑</button><button data-move="${i},1" aria-label="Move ${escape(name(id))} down" ${i===state.order.length-1?'disabled':''}>↓</button></div></li>`).join('')}</ol>${state.saved?'<label>What changed?<textarea id="reason" maxlength="2000" placeholder="Explain why you are updating your preferences."></textarea></label>':''}<div class="actions"><button class="primary" id="save">${state.saved?'Update':'Submit'} preferences ↗</button><span class="badge">${state.order.length} ranked</span></div><div id="assignment"></div>`}`;
$('#exitWorkspace').onclick=()=>logout();
$('#round').onchange=async e=>{state.round=Number(e.target.value);try{await loadRound();}catch(error){notify(error.message);}};if(!state.round)return;if(admin){$('#assign').onclick=assign;$('#refresh').onclick=()=>overview().catch(e=>notify(e.message));renderAdminAssignment();return;}
document.querySelectorAll('[data-move]').forEach(b=>b.onclick=()=>{const[i,d]=b.dataset.move.split(',').map(Number);[state.order[i],state.order[i+d]]=[state.order[i+d],state.order[i]];render();});document.querySelectorAll('.rank-item').forEach(row=>{row.ondragstart=e=>{dragId=Number(row.dataset.id);e.dataTransfer.setData('text/plain',String(dragId));};row.ondragover=e=>e.preventDefault();row.ondrop=e=>{e.preventDefault();const at=state.order.indexOf(Number(row.dataset.id)),from=state.order.indexOf(dragId);if(at<0||from<0)return;state.order.splice(at,0,state.order.splice(from,1)[0]);render();};});$('#save').onclick=save;renderAssignment();}
async function save(){const button=$('#save');button.disabled=true;try{const reason=$('#reason')?.value??'';if(state.saved&&!reason.trim())throw Error('Please explain why your preferences changed.');if(!state.demo)await api(`/rounds/${state.round}/preferences`,'PUT',{targets:state.order,reason});state.saved=true;render();notify(state.demo?'Demo preferences updated. No server data changed.':'Preferences submitted. Your administrator can now include them in matching.');}catch(e){notify(e.message);}finally{if($('#save'))$('#save').disabled=false;}}
function renderAssignment(){
if(state.assignmentError){$('#assignment').innerHTML='<div class="assignment error">Could not load your saved assignment. <button class="secondary" id="retryAssignment">Try again</button></div>';$('#retryAssignment').onclick=()=>loadRound().catch(e=>notify(e.message));return;}
if(!state.run){$('#assignment').innerHTML='<div class="assignment"><strong>No published assignment for this round yet.</strong><p>Your preferences are saved separately. Your administrator creates the matching.</p></div>';return;}
const pair=state.run.assignments[0];$('#assignment').innerHTML=`<div class="assignment"><strong>${pair?'Your mentoring partner: '+escape(name(pair.mentor_id===state.user.id?pair.mentee_id:pair.mentor_id)):'You are currently unassigned.'}</strong>${!state.run.inputs_are_current?'<p>Preferences have changed since this assignment was checked. Your administrator can recheck it.</p>':''}${pair?'<label>Private feedback for your administrator<select id="score"><option value="5">5 — Excellent</option><option value="4">4 — Good</option><option value="3">3 — Okay</option><option value="2">2 — Needs improvement</option><option value="1">1 — Poor</option></select></label><label>Comment<textarea id="feedback" maxlength="2000"></textarea></label><button id="rate" class="secondary">Submit feedback</button>':''}</div>`;if(pair)$('#rate').onclick=async()=>{try{await api('/ratings','POST',{assignment_id:pair.id,score:Number($('#score').value),comment:$('#feedback').value});$('#rate').disabled=true;notify('Feedback sent privately to your administrator.');}catch(e){notify(e.message);}};}
async function overview(){let data;if(state.demo){data={participants:state.people.map(p=>({...p,targets:state.people.filter(q=>q.role!==p.role).map(q=>q.user_id)})),ready:true,missing:0};}else{data=await api(`/rounds/${state.round}/preferences-overview`);}for(const role of ['mentor','mentee']){const target=$('#'+role+'s');if(!target)return;target.innerHTML=data.participants.filter(p=>p.role===role).map(p=>`<details class="overview"><summary>${escape(p.name)} <span class="badge">${p.targets===null?'Awaiting preferences':'Submitted'}</span></summary>${p.targets===null?'<p class="subtext">No submitted rankings yet.</p>':`<ol>${p.targets.map(id=>`<li>${escape(name(id))}</li>`).join('')}</ol>`}</details>`).join('')||'<p class="subtext">No participants yet.</p>';}
$('#readiness').textContent=data.ready?'Everyone has submitted their preferences. Ready to assign.':`${data.missing} preference lists outstanding. The round must also be frozen before matching.`;$('#assign').disabled=!data.ready;await loadAdminHistory();}
function confirmAssignment(){
  const dialog=$('#confirmAssignment');
  return new Promise(resolve=>{
    const finish=accepted=>{dialog.close();resolve(accepted);};
    $('#closeConfirmation').onclick=()=>finish(false);
    $('#cancelAssignment').onclick=()=>finish(false);
    $('#confirmAssignmentButton').onclick=()=>finish(true);
    dialog.oncancel=e=>{e.preventDefault();finish(false);};
    dialog.onclick=e=>{if(e.target===dialog)finish(false);};
    dialog.showModal();
  });
}
// Restore results from persisted runs on login, round changes and Refresh.
async function loadAdminHistory(){
  const round=state.round, token=state.token;
  const results=state.demo?[
    {status:'fulfilled',value:{runs:state.runs}},
    {status:'fulfilled',value:{ratings:[]}}
  ]:await Promise.allSettled([api(`/rounds/${round}/matches`),api('/ratings')]);
  if(state.role!=='admin'||state.round!==round||state.token!==token)return;
  if(results[0].status==='fulfilled'){
    state.runs=results[0].value.runs;
    state.run=[...state.runs].reverse().find(run=>run.state==='verified')??null;
    renderAdminAssignment();
  }else{
    $('#matchResult').innerHTML='<div class="assignment error">Could not load saved assignments. Use Refresh to retry.</div>';
  }
  if(results[0].status==='fulfilled'&&results[1].status==='fulfilled'){
    // Ratings belong to assignment IDs. Only IDs from this round may be shown.
    const assignments=new Set(state.runs.flatMap(run=>run.assignments.map(pair=>pair.id)));
    state.ratings=results[1].value.ratings.filter(rating=>assignments.has(rating.assignment_id));
    renderFeedback();
  }else{
    $('#feedbackPanel').innerHTML='<h2 id="feedbackHeading">Feedback From:</h2><p class="error">Could not load feedback for this round. Use Refresh above to retry.</p>';
  }
  sizeFeedbackPanel();
}
function renderAdminAssignment(){
  const target=$('#matchResult');if(!target)return;
  if(!state.run){target.innerHTML='<div class="assignment"><strong>No published assignment for this round yet.</strong></div>';return;}
  const run=state.run;
  const latest=state.runs[state.runs.length-1];
  target.innerHTML=`<div class="assignment"><strong>Saved mentoring assignments</strong><p class="subtext">${state.demo?'Sample assignment':`Run ${escape(run.id)} · Verified for its saved preferences`}</p>${run.inputs_are_current===false?'<p>Preferences have changed since this assignment was checked. These are the saved pairs.</p>':''}${latest&&latest.state!=='verified'?'<p class="error">The latest run did not pass verification. The previously verified pairs are shown below.</p>':''}<div class="columns">${['mentor','mentee'].map(role=>`<section><h3>${role==='mentor'?'Mentors':'Mentees'}</h3>${state.people.filter(person=>person.role===role).map(person=>{const pair=run.assignments.find(pair=>(role==='mentor'?pair.mentor_id:pair.mentee_id)===person.user_id);return `<p><strong>${escape(person.name)}</strong><br>${pair?escape(name(role==='mentor'?pair.mentee_id:pair.mentor_id)):'Unassigned'}</p>`;}).join('')}</section>`).join('')}</div></div>`;
}
function renderFeedback(){
  const panel=$('#feedbackPanel');
  if(state.role!=='admin'||!state.round){panel.hidden=true;return;}
  panel.hidden=false;
  panel.innerHTML=`<p class="eyebrow">ADMINISTRATOR ONLY</p><h2 id="feedbackHeading">Feedback From:</h2><p class="subtext">Private feedback for ${escape(state.rounds.find(round=>round.id===state.round)?.name??'this round')}.</p>${state.demo?'<div class="demo-banner">Sample workspace · No real feedback is loaded.</div>':''}<div class="columns">${['mentor','mentee'].map(role=>{
    const people=new Set(state.people.filter(person=>person.role===role).map(person=>person.user_id));
    const ratings=state.ratings.filter(rating=>people.has(rating.author_id));
    return `<section><h3>${role==='mentor'?'Mentors':'Mentees'} <span class="badge">${ratings.length}</span></h3>${ratings.length?ratings.map(rating=>{
      const run=state.runs.find(run=>run.assignments.some(pair=>pair.id===rating.assignment_id));
      return `<article class="feedback-card"><div class="feedback-card-heading"><strong>${escape(name(rating.author_id))}</strong><span class="badge">${escape(rating.score)} / 5</span></div><p class="subtext">About ${escape(name(rating.target_id))} · Run ${escape(run?.id??'')}${run?.id!==state.run?.id?' · Previous assignment':''}</p><p class="feedback-comment">${rating.comment?escape(rating.comment):'No written comment provided.'}</p></article>`;
    }).join(''):'<p class="subtext">No feedback submitted yet.</p>'}</section>`;
  }).join('')}</div>`;
  sizeFeedbackPanel();
}
function sizeFeedbackPanel(){
  const panel=$('#feedbackPanel');
  if(!panel.hidden)panel.style.minHeight=Math.ceil($('#workspace').getBoundingClientRect().height)+'px';
}
if(typeof ResizeObserver!=='undefined')new ResizeObserver(sizeFeedbackPanel).observe($('#workspace'));
async function assign(){
  if(!await confirmAssignment())return;
  const round=state.round;$('#assign').disabled=true;
  try{
    const run=state.demo?{id:1,state:'verified',inputs_are_current:true,assignments:[{id:1,mentor_id:10,mentee_id:40},{id:2,mentor_id:20,mentee_id:50}]}:await api(`/rounds/${round}/matches`,'POST');
    if(state.round!==round||state.role!=='admin')return;
    state.run=run.state==='verified'?run:state.run;state.runs.push(run);
    renderAdminAssignment();await loadAdminHistory();
    notify(state.demo?'Demo matching shown. No algorithm was run.':'Matching and independent verification completed.');
  }catch(e){notify(e.message);}finally{if($('#assign'))$('#assign').disabled=false;}
}

// Match the background SVG's xMidYMid slice transform. Anchor each figure's
// feet to a real point on the illustrated path, even when the desktop resizes.
function positionClimbers() {
  const scale = Math.max(innerWidth / 1600, innerHeight / 1000);
  const offsetX = (innerWidth - 1600 * scale) / 2;
  const offsetY = (innerHeight - 1000 * scale) / 2;
  for (const [selector, x, y] of [['.mentee',461.6,847.2], ['.mentor',1086.875,562.5]]) {
    const figure = document.querySelector(selector);
    const drawingHeight = figure.querySelector('svg').getBoundingClientRect().height || 120;
    figure.style.left = (offsetX + x * scale) + 'px';
    figure.style.top = (offsetY + y * scale - drawingHeight * 124 / 140) + 'px';
    figure.style.right = 'auto';
    figure.style.bottom = 'auto';
  }
  // Align the instruction with the start of 'Rise together', below the headline.
  if (!document.body.classList.contains('logged-in')) {
    const anchor = $('#instructionAnchor').getBoundingClientRect();
    $('#choosePlace').style.left = anchor.left + 'px';
    $('#choosePlace').style.top = (anchor.bottom + 24) + 'px';
  }
}
addEventListener('resize', positionClimbers);
positionClimbers();
