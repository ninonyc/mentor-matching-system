// Optional real-browser checks. Requires Playwright and its Chromium browser.
const assert = require('node:assert/strict');
const path = require('node:path');
const {spawn} = require('node:child_process');
const {createInterface} = require('node:readline');
const ROOT = path.resolve(__dirname, '../..');
let chromium;
try { ({chromium} = require('playwright')); }
catch { ({chromium} = require(path.join(ROOT, 'build/phase7-browser/node_modules/playwright'))); }

async function main() {
  const fixture = spawn(process.env.PYTHON || 'python3', ['tools/phase7_demo.py', '--automated'],
    {cwd:ROOT, stdio:['pipe','pipe','inherit']});
  let browser;
  try {
    const data = await new Promise((resolve,reject)=>{
      const lines = createInterface({input:fixture.stdout});
      const timer=setTimeout(()=>reject(Error('Fixture startup timed out')),45000);
      fixture.once('error',error=>{clearTimeout(timer);reject(error);});
      fixture.once('exit',code=>{clearTimeout(timer);reject(Error('Fixture exited: '+code));});
      lines.once('line',line=>{clearTimeout(timer);try{resolve(JSON.parse(line));}catch(e){reject(e);}});
    });
    browser=await chromium.launch({headless:true,
      ...(process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE ? {executablePath:process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE} : {})});
    const page=await browser.newPage({viewport:{width:1440,height:900}});
    const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto(data.origin);
    async function login(role,email) {
      await page.click(role==='admin'?'#adminLink':'.entrance[data-role="'+role+'"]');
      await page.fill('[name=email]',email);await page.fill('[name=password]',data.password);
      await page.click('#signIn');await page.waitForFunction(()=>!document.querySelector('#workspace').hidden && !document.querySelector('#login').open);
      assert.equal(await page.locator('.demo-banner').count(),0);
    }
    async function exit(){await page.click('#exitWorkspace');await page.waitForFunction(()=>document.querySelector('#workspace').hidden);}
    async function api(route,method='GET',body,token) {
      const res=await fetch(data.base+'/api'+route,{method,headers:{'Content-Type':'application/json',...(token?{Authorization:'Bearer '+token}:{})},...(body?{body:JSON.stringify(body)}:{})});
      assert.ok(res.ok,route+': '+res.status);return res.json();
    }
    const admin=(await api('/auth/login','POST',{email:data.admin,password:data.password})).token;
    const prefix='/rounds/'+data.round_id;
    // Dismiss each role entrance without submitting credentials.
    for(const role of ['mentor','mentee','admin']){
      const entrance=role==='admin'?'#adminLink':'.entrance[data-role="'+role+'"]';
      await page.click(entrance);await page.click('#backFromLogin');assert.equal(await page.locator('#login').evaluate(e=>e.open),false);
      await page.click(entrance);await page.click('#login .close');assert.equal(await page.locator('#login').evaluate(e=>e.open),false);
      await page.click(entrance);await page.keyboard.press('Escape');assert.equal(await page.locator('#login').evaluate(e=>e.open),false);
    }
    const alignment=await page.evaluate(()=>{const a=document.querySelector('#instructionAnchor').getBoundingClientRect(),b=document.querySelector('#choosePlace').getBoundingClientRect();return {dx:Math.abs(a.left-b.left),below:b.top>=a.bottom};});
    assert.ok(alignment.dx<1&&alignment.below,'Instruction aligned below Rise together');
    assert.equal(await page.locator('.summit').count(),0);
    for(const selector of ['.mentor','.mentee']){
      const before=await page.locator(selector).evaluate(e=>e.getBoundingClientRect().top);
      await page.hover(selector);await page.waitForTimeout(300);
      const after=await page.locator(selector).evaluate(e=>e.getBoundingClientRect().top);
      assert.ok(before-after>6,'Hover lifts '+selector);await page.mouse.move(0,0);await page.waitForTimeout(300);
    }
    await login('admin',data.admin);assert.equal(await page.locator('#assign').isDisabled(),true);await exit();
    for(const person of data.participants){
      await login(person.role,person.email);
      const first=await page.locator('.rank-item .identity').first().innerText();
      await page.click('[data-move="0,1"]');await page.click('[data-move="1,-1"]');
      assert.equal(await page.locator('.rank-item .identity').first().innerText(),first);
      await page.click('#save');await page.waitForFunction(()=>document.querySelector('#reason')!==null);
      assert.match(await page.locator('#notice').innerText(),/submitted/);
      await exit();
    }
    await login('admin',data.admin);await page.waitForFunction(()=>!document.querySelector('#assign').disabled);
    for(const control of ['#closeConfirmation','#cancelAssignment','Escape']){
      await page.click('#assign');if(control==='Escape')await page.keyboard.press('Escape');else await page.click(control);
      assert.equal((await api(prefix+'/matches','GET',null,admin)).runs.length,0);
    }
    await page.click('#assign');await page.click('#confirmAssignmentButton');
    await page.waitForFunction(()=>document.querySelector('#matchResult').textContent.includes('Saved mentoring assignments'));
    const runs=(await api(prefix+'/matches','GET',null,admin)).runs;
    assert.equal(runs.length,1);assert.equal(runs[0].verification.status,'Stable');assert.equal(runs[0].assignments.length,2);await exit();
    for(const person of data.participants.filter(p=>p.role==='mentee')){
      await login('mentee',person.email);await page.waitForSelector('#rate');
      await page.fill('#feedback','Phase 7 browser feedback');await page.click('#rate');
      await page.waitForFunction(()=>document.querySelector('#rate').disabled);await exit();
    }
    const sam=data.participants.find(p=>p.email==='sam@ascent.test');
    await login('mentor',sam.email);assert.match(await page.locator('#assignment').innerText(),/currently unassigned/);await exit();
    assert.equal((await api('/ratings','GET',null,admin)).ratings.length,2);
    await login('admin',data.admin);
    await page.waitForFunction(()=>document.querySelector('#feedbackPanel').textContent.includes('Phase 7 browser feedback'));
    assert.match(await page.locator('#matchResult').innerText(),/Saved mentoring assignments/);
    assert.match(await page.locator('#feedbackPanel').innerText(),/Feedback From:/);
    const bounds=await page.evaluate(()=>{const a=document.querySelector('#workspace').getBoundingClientRect(),b=document.querySelector('#feedbackPanel').getBoundingClientRect();return {sameWidth:Math.abs(a.width-b.width)<1,below:b.top>=a.bottom};});
    assert.ok(bounds.sameWidth&&bounds.below,'Feedback is a separate, equally wide lower panel');
    await page.reload();await login('admin',data.admin);
    await page.waitForFunction(()=>document.querySelector('#feedbackPanel').textContent.includes('Phase 7 browser feedback'));
    assert.match(await page.locator('#matchResult').innerText(),/Saved mentoring assignments/);await exit();
    assert.deepEqual(errors,[],'No uncaught frontend errors');
    console.log('PHASE 7 BROWSER WORKFLOW PASSED');
  } finally {
    if(browser)await browser.close();fixture.stdin.end('\n');
    await new Promise(resolve=>{if(fixture.exitCode!==null)return resolve();const timer=setTimeout(()=>fixture.kill('SIGTERM'),10000);fixture.once('exit',()=>{clearTimeout(timer);resolve();});});
  }
}
main().catch(e=>{console.error(e);process.exitCode=1;});
