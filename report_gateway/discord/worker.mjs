// ProTanki Editor PRO Discord gateway 0.5.27.
// Settings > Variables and secrets: DISCORD_WEBHOOK_URL (Secret).
// Settings > Bindings: REPORT_LIMITS (KV namespace protanki-report-limits).
// NEVER embed the Discord webhook URL in a distributable application.
const WINDOW_SECONDS = 1800;
const accepted = () => new Response(JSON.stringify({received:true}), {
  status:202, headers:{'Content-Type':'application/json','Cache-Control':'no-store'}
});
const error = (message, status) => new Response(message, {
  status, headers:{'Cache-Control':'no-store'}
});
async function hmac(secret, value) {
  const key=await crypto.subtle.importKey('raw',new TextEncoder().encode(secret),
    {name:'HMAC',hash:'SHA-256'},false,['sign']);
  const bytes=await crypto.subtle.sign('HMAC',key,new TextEncoder().encode(value));
  return Array.from(new Uint8Array(bytes),b=>b.toString(16).padStart(2,'0')).join('');
}
export default {
  async fetch(request,env) {
    const url=new URL(request.url);
    if(url.pathname!=='/api/report')return error('Not found',404);
    if(request.method!=='POST')return error('Method not allowed',405);
    if(!env.DISCORD_WEBHOOK_URL || !env.REPORT_LIMITS)return error('Reporting unavailable',503);
    const ip=request.headers.get('CF-Connecting-IP');
    if(!ip)return error('Reporting unavailable',503);
    if(Number(request.headers.get('Content-Length')||0)>100000)return error('Report too large',413);
    let report;
    try {
      const raw=await request.text();
      if(new TextEncoder().encode(raw).length>100000)return error('Report too large',413);
      report=JSON.parse(raw);
    }catch{return error('Invalid JSON',400);}
    if(!report || typeof report!=='object' || Array.isArray(report) ||
       !['0.5.26','0.5.27'].includes(report.version) ||
       typeof report.client_id!=='string'||!/^[a-f0-9]{32}$/.test(report.client_id)||
       typeof report.subject!=='string'||!report.subject.trim()||report.subject.length>160||
       typeof report.description!=='string'||!report.description.trim()||report.description.length>4000||
       typeof report.logs_opt_in!=='boolean'||typeof report.logs!=='string'||report.logs.length>70000||
       (!report.logs_opt_in&&report.logs.length!==0))return error('Invalid report',400);

    // Two independent, salted private KV keys: same installation and same IP.
    // Both checks return the SAME 202 response during cooldown (silent UI).
    const salt=env.REPORT_HASH_KEY||env.DISCORD_WEBHOOK_URL;
    const installationKey='install:'+await hmac(salt,'install:'+report.client_id);
    const addressKey='ip:'+await hmac(salt,'ip:'+ip);
    // Honor still-active 0.5.26 dashboard Worker entries during migration;
    // legacy IP hashes expire naturally, and new entries are always HMAC-keyed.
    const oldBytes=await crypto.subtle.digest('SHA-256',new TextEncoder().encode(ip));
    const legacyKey='report:'+Array.from(new Uint8Array(oldBytes),b=>b.toString(16).padStart(2,'0')).join('');
    try {
      const [installation,address,legacy]=await Promise.all([
        env.REPORT_LIMITS.get(installationKey),env.REPORT_LIMITS.get(addressKey),env.REPORT_LIMITS.get(legacyKey)
      ]);
      if(installation || address || legacy)return accepted();
    }catch{return error('Reporting unavailable',503);}

    const payload={
      username:'ProTanki Editor PRO',allowed_mentions:{parse:[]},
      embeds:[{
        title:'Bug Report - '+report.subject.slice(0,160),
        description:report.description,color:3447003,
        fields:[
          {name:'Editor version',value:report.version,inline:true},
          {name:'Session logs',value:report.logs_opt_in&&report.logs?'Attached':'Not attached',inline:true}
        ],timestamp:new Date().toISOString()
      }]
    };
    const form=new FormData();form.append('payload_json',JSON.stringify(payload));
    if(report.logs_opt_in && report.logs){
      form.append('files[0]',new Blob([report.logs],{type:'text/plain;charset=utf-8'}),'session-logs.txt');
    }
    try {
      const webhook=new URL(env.DISCORD_WEBHOOK_URL);
      if(webhook.protocol!=='https:'||webhook.hostname!=='discord.com')return error('Reporting unavailable',503);
      webhook.searchParams.set('wait','true');
      const response=await fetch(webhook.toString(),{method:'POST',body:form});
      if(!response.ok)return error('Discord delivery failed',502);
      // TTL is enforced by KV; library paths, user emails and raw IP are never stored.
      await Promise.all([
        env.REPORT_LIMITS.put(installationKey,'1',{expirationTtl:WINDOW_SECONDS}),
        env.REPORT_LIMITS.put(addressKey,'1',{expirationTtl:WINDOW_SECONDS})
      ]);
      return accepted();
    }catch{return error('Delivery unavailable',503);}
  }
};
