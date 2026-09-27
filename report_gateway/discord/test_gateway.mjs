import assert from 'node:assert/strict';
import worker from './worker.mjs';
const entries=new Map(),messages=[];
const env={DISCORD_WEBHOOK_URL:'https://discord.com/api/webhooks/1/PRIVATE_TOKEN',REPORT_LIMITS:{
  async get(key){return entries.get(key)||null;},
  async put(key,value,{expirationTtl}){assert.equal(expirationTtl,1800);entries.set(key,value);}
}};
const realFetch=globalThis.fetch;
globalThis.fetch=async(url,opts)=>{
 assert.match(String(url),/^https:\/\/discord\.com\/api\/webhooks\/1\/PRIVATE_TOKEN\?wait=true$/);
 assert.equal(opts.method,'POST'); const p=JSON.parse(opts.body.get('payload_json'));
 messages.push({p,logs:opts.body.get('files[0]')});return new Response('{}',{status:200});
};
const base={version:'0.5.28',client_id:'0123456789abcdef0123456789abcdef',subject:'Test',
 description:'Testing Discord',logs_opt_in:false,logs:''};
const req=(obj,ip='192.0.2.1')=>new Request('https://unit.test/api/report',{method:'POST',
 headers:{'Content-Type':'application/json','CF-Connecting-IP':ip},body:JSON.stringify(obj)});
try {
 assert.equal((await worker.fetch(req(base),env)).status,202);
 assert.equal(messages.length,1);
 assert.equal((await worker.fetch(req(base),env)).status,202);
 assert.equal(messages.length,1,'repeated requests must be silently suppressed');
 assert.equal(entries.size,2,'both install and IP keys must be stored');
 const legacyBytes=await crypto.subtle.digest('SHA-256',new TextEncoder().encode('192.0.2.100'));
 const legacyKey='report:'+Array.from(new Uint8Array(legacyBytes),b=>b.toString(16).padStart(2,'0')).join('');
 entries.set(legacyKey,'1');
 assert.equal((await worker.fetch(req({...base,client_id:'cccccccccccccccccccccccccccccccc'},'192.0.2.100'),env)).status,202);
 assert.equal(messages.length,1,'active legacy IP cooldown must be honored on upgrade');
 assert.equal((await worker.fetch(req({...base,client_id:'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'}),env)).status,202);
 assert.equal(messages.length,1,'changed client ID must not bypass IP cooldown');
 assert.equal((await worker.fetch(req({...base,client_id:'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'},'192.0.2.2'),env)).status,202);
 assert.equal(messages.length,2,'other IP and other client may send');
 assert.equal((await worker.fetch(req({...base,logs_opt_in:false,logs:'stolen'}),env)).status,400);
 assert.equal((await worker.fetch(req({...base,version:'bad'}),env)).status,400);
 assert.equal((await worker.fetch(req({...base,client_id:'invalid'}),env)).status,400);
 assert.equal((await worker.fetch(req({...base,client_id:'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',logs_opt_in:true,logs:'redacted log'},'192.0.2.3'),env)).status,202);
 assert.equal(messages[2].logs.name,'session-logs.txt');
 assert.equal(await messages[2].logs.text(),'redacted log');
 assert.deepEqual(messages[2].p.allowed_mentions,{parse:[]});
 assert.equal((await worker.fetch(new Request('https://unit.test/'),env)).status,404);
 assert.equal((await worker.fetch(new Request('https://unit.test/api/report'),env)).status,405);
 console.log('PASS: Discord delivery, opt-in log attachment, validation, per-IP + installation silent 30-minute KV cooldown');
}finally{globalThis.fetch=realFetch;}
