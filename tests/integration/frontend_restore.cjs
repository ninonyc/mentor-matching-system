// Dependency-free controller regression tests; these do not replace browser QA.
const assert=require('node:assert/strict');
const {test}=require('node:test');
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const source=fs.readFileSync(path.resolve(__dirname,'../../frontend/app.js'),'utf8');
const people=[{user_id:10,name:'Alex',role:'mentor'},{user_id:20,name:'Sam',role:'mentor'},{user_id:40,name:'Jordan',role:'mentee'}];
const run={id:7,state:'verified',inputs_are_current:true,assignments:[{id:70,mentor_id:10,mentee_id:40}]};
const ratings=[{id:1,assignment_id:70,author_id:10,target_id:40,score:4,comment:'Mentor comment'},
{id:2,assignment_id:70,author_id:40,target_id:10,score:5,comment:'<img src=x onerror=alert(1)>\nMentee comment'},
{id:3,assignment_id:999,author_id:40,target_id:10,score:1,comment:'OTHER ROUND SECRET'}];
function harness(role='admin',options={}){
  const elements=new Map(),calls=[];
  const element=selector=>{
    if(!elements.has(selector))elements.set(selector,{innerHTML:'',textContent:'',hidden:false,style:{},disabled:false,
      addEventListener(){},querySelector(){return element('svg');},getBoundingClientRect(){return {left:100,bottom:200,height:450};}});
    return elements.get(selector);
  };
  const body={className:'',classList:{contains:c=>body.className.split(' ').includes(c)}};
  const sandbox={location:options.location??{hostname:'127.0.0.1',port:'5173'},document:{body,querySelector:element,querySelectorAll:()=>[]},innerWidth:1440,innerHeight:900,
    addEventListener(){},setTimeout(){},clearTimeout(){},console,
    fetch:async url=>{
      const route=url.startsWith('/api')?url.slice(4):url.replace('http://127.0.0.1:8080/api','');calls.push(route);
      let data,status=200;
      if(route.endsWith('/participants'))data={participants:people};
      else if(route.endsWith('/preferences-overview'))data={participants:people.map(p=>({...p,targets:[]})),ready:true,missing:0};
      else if(route.endsWith('/my-assignment')){
        data=options.noAssignment?{error:'No verified matching yet'}:options.unmatched?{...run,assignments:[]}:run;
        if(options.noAssignment)status=404;
        if(options.assignmentError){status=500;data={error:'Database unavailable'};}
      }else if(route.endsWith('/preferences'))data={targets:role==='mentor'?[40]:[10,20]};
      else if(route.endsWith('/matches')){data={runs:options.noAssignment?[]:[run]};if(options.historyError){status=500;data={error:'Cannot read history'};}}
      else if(route==='/ratings'){data={ratings};if(options.ratingsError){status=500;data={error:'Cannot read ratings'};}}
      else throw Error('Unexpected route '+route);
      return {ok:status<400,status,json:async()=>data};
    }};
  const context=vm.createContext(sandbox);vm.runInContext(source,context);
  vm.runInContext(`state={role:${JSON.stringify(role)},token:'test',demo:false,user:{id:${role==='mentor'?20:40}},rounds:[{id:1,name:'Test round'}],round:1,people:[],order:[],saved:false,run:null,runs:[],ratings:[]}`,context);
  return {element,calls,context,load:()=>vm.runInContext('loadRound()',context)};
}
test('fresh admin login restores saved pairs and groups feedback for selected round',async()=>{
  const h=harness();await h.load();
  assert.match(h.element('#matchResult').innerHTML,/Saved mentoring assignments/);
  assert.match(h.element('#matchResult').innerHTML,/Alex/);
  const panel=h.element('#feedbackPanel').innerHTML;
  assert.match(panel,/Feedback From:/);assert.match(panel,/Mentors/);assert.match(panel,/Mentees/);
  assert.match(panel,/Mentor comment/);assert.match(panel,/Mentee comment/);
  assert.ok(panel.indexOf('Mentor comment')<panel.indexOf('Mentees'));
  assert.ok(panel.indexOf('Mentee comment')>panel.indexOf('Mentees'));
  assert.ok(!panel.includes('OTHER ROUND SECRET'));
  assert.ok(panel.includes('&lt;img'));assert.ok(!panel.includes('<img'));
  assert.equal(h.element('#feedbackPanel').style.minHeight,'450px');
  // A second new controller represents a refresh/relogin with server data intact.
  const fresh=harness();await fresh.load();assert.equal(fresh.element('#matchResult').innerHTML,h.element('#matchResult').innerHTML);
});
test('participant login reloads partner and never fetches administrator feedback',async()=>{
  const h=harness('mentee');await h.load();
  assert.match(h.element('#assignment').innerHTML,/Your mentoring partner: Alex/);
  assert.ok(h.calls.includes('/rounds/1/my-assignment'));assert.ok(!h.calls.includes('/ratings'));
  assert.equal(h.element('#feedbackPanel').hidden,true);
});
test('published unmatched and no published assignment are distinct states',async()=>{
  const h=harness('mentor',{unmatched:true});await h.load();assert.match(h.element('#assignment').innerHTML,/currently unassigned/);
  const missing=harness('mentee',{noAssignment:true});await missing.load();assert.match(missing.element('#assignment').innerHTML,/No published assignment/);
});
test('assignment request failure shows retry rather than pretending data is absent',async()=>{
  const h=harness('mentee',{assignmentError:true});await h.load();assert.match(h.element('#assignment').innerHTML,/Could not load/);assert.match(h.element('#assignment').innerHTML,/Try again/);
});
test('ratings failure is explicit without hiding saved assignments',async()=>{
  const h=harness('admin',{ratingsError:true});await h.load();assert.match(h.element('#matchResult').innerHTML,/Saved mentoring assignments/);assert.match(h.element('#feedbackPanel').innerHTML,/Could not load feedback/);
});
test('history failure cannot be mislabeled as no saved matching or no feedback',async()=>{
  const h=harness('admin',{historyError:true});await h.load();assert.match(h.element('#matchResult').innerHTML,/Could not load saved assignments/);assert.match(h.element('#feedbackPanel').innerHTML,/Could not load feedback/);
});

test('hosted and container pages use their own origin, while static development stays local',async()=>{
  const hosted=harness('mentee',{location:{hostname:'ascent.onrender.com',port:''}});await hosted.load();
  assert.equal(vm.runInContext('apiBase',hosted.context),'/api');
  assert.match(hosted.element('#assignment').innerHTML,/Your mentoring partner: Alex/);
  const container=harness('admin',{location:{hostname:'localhost',port:'10000'}});await container.load();
  assert.equal(vm.runInContext('apiBase',container.context),'/api');
  assert.equal(vm.runInContext('apiBase',harness().context),'http://127.0.0.1:8080/api');
});
