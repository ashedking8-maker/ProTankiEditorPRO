import assert from 'node:assert/strict';
import worker, {ReportGate} from './worker.mjs';
const instances=new Map(),email=[];
const objectFor=id=>{
  if(instances.has(id))return instances.get(id);
  const values=new Map();
  const storage={get:async k=>values.get(k),put:async(k,v)=>values.set(k,v),delete:async k=>values.delete(k),transaction:async fn=>fn(storage)};
  const gate=new ReportGate({storage});
  const stub={fetch:(url,options)=>gate.fetch(new Request(url,options))};
  instances.set(id,{values,stub});return instances.get(id);
};
const originalFetch=globalThis.fetch;
globalThis.fetch=async (url,options)=>{
  assert.match(String(url),/^https:\/\/api\.resend\.com\/emails$/);
  email.push(JSON.parse(options.body));return new Response('{}',{status:200});
};
const env={REPORT_GATE:{idFromName:v=>v,get:id=>objectFor(id).stub},
  REPORT_TO_EMAIL:'private-test@example.org',REPORT_FROM_EMAIL:'ptpro@sender.example.org',
  RESEND_API_KEY:'server-only',REPORT_HASH_KEY:'secret-test-only'};
const body={version:'0.5.25',client_id:'a'.repeat(32),subject:'Crash',description:'Open 3DS then frame model',logs_opt_in:false,logs:''};
const req=(ip='203.0.113.10')=>new Request('https://endpoint.example/api/report',{method:'POST',
  headers:{'CF-Connecting-IP':ip,'Content-Type':'application/json'},body:JSON.stringify(body)});
assert.equal((await worker.fetch(req(),env)).status,202);
let response=await worker.fetch(req(),env);assert.equal(response.status,202);
assert.equal(email.length,1,'repeated reports produce no second email');
assert.equal((await response.text()).includes('private-test'),false,'recipient not exposed to client');
body.client_id='b'.repeat(32);
assert.equal((await worker.fetch(req(),env)).status,202);
assert.equal(email.length,1,'new installation from the same address is still limited');
body.client_id='a'.repeat(32);
assert.equal((await worker.fetch(req('203.0.113.11'),env)).status,202);
assert.equal(email.length,1,'same installation from new address is still limited');
body.client_id='c'.repeat(32);
assert.equal((await worker.fetch(req('203.0.113.11'),env)).status,202);
assert.equal(email.length,2,'new installation and new IP can submit');
assert.equal(email[0].to[0],env.REPORT_TO_EMAIL);
// Expired persistent gates must permit a new email rather than blocking forever.
for(const {values} of instances.values()){
  const last=values.get('last');
  if(last)values.set('last',{...last,time:Date.now()-30*60*1000-1000});
}
assert.equal((await worker.fetch(req('203.0.113.11'),env)).status,202);
assert.equal(email.length,3,'expired 30-minute gates permit the next report');
console.log('PASS: 30-minute per-installation and per-IP throttles, private destination, duplicate acceptance');
globalThis.fetch=originalFetch;
