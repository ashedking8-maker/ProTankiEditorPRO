// Optional report gateway. Not bundled or executed by the Windows editor.
// Server-held secrets: REPORT_TO_EMAIL, REPORT_FROM_EMAIL, RESEND_API_KEY,
// REPORT_HASH_KEY. Recipient address never appears in the client binary/API.
const WINDOW_MS = 30 * 60 * 1000;
const text = (v) => new Response(JSON.stringify(v), {
  status: 202, headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' }
});
async function hmac(secret, value) {
  const key = await crypto.subtle.importKey('raw', new TextEncoder().encode(secret),
    { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  const signed = await crypto.subtle.sign('HMAC', key, new TextEncoder().encode(value));
  return Array.from(new Uint8Array(signed), b => b.toString(16).padStart(2, '0')).join('');
}
export class ReportGate {
  constructor(state) { this.state = state; }
  async fetch(request) {
    if(request.method!=='POST') return new Response('Method not allowed',{status:405});
    const input=await request.json();
    if(input.action==='release') {
      await this.state.storage.transaction(async tx => {
        const last=await tx.get('last');
        if(last?.receipt===input.receipt)await tx.delete('last');
      });
      return text({ received:true });
    }
    let granted=false,receipt='';
    await this.state.storage.transaction(async tx => {
      const last=await tx.get('last');
      if(last && Date.now()-last.time<WINDOW_MS){
        await tx.put('duplicate_count',(await tx.get('duplicate_count')||0)+1);
      } else {
        granted=true;receipt=crypto.randomUUID();
        await tx.put('last',{time:Date.now(),receipt});
      }
    });
    // Duplicate reports are accepted and counted, but no additional email is
    // dispatched during the window. UI says 'received', not 'email sent'.
    return text({ received:true,deliver:granted,receipt });
  }
}
export default {
  async fetch(request, env) {
    if(request.method!=='POST'||new URL(request.url).pathname!=='/api/report')
      return new Response('Not found',{status:404});
    if(!env.REPORT_TO_EMAIL||!env.REPORT_FROM_EMAIL||!env.RESEND_API_KEY||!env.REPORT_HASH_KEY)
      return new Response('Service not configured',{status:503});
    const ip=request.headers.get('CF-Connecting-IP');
    if(!ip) return new Response('Client address unavailable',{status:503});
    const length=Number(request.headers.get('Content-Length')||0);
    if(length>100000) return new Response('Payload too large',{status:413});
    let body;
    try{
      const raw=await request.text();if(raw.length>100000)return new Response('Payload too large',{status:413});
      body=JSON.parse(raw);
    }catch{return new Response('Malformed JSON',{status:400});}
    if(body?.version!=='0.5.25'||typeof body.client_id!=='string'||!/^[a-f0-9]{32}$/.test(body.client_id)||
       typeof body.subject!=='string'||!body.subject.trim()||body.subject.length>160||
       typeof body.description!=='string'||!body.description.trim()||body.description.length>4000||
       typeof body.logs!=='string'||body.logs.length>70000||typeof body.logs_opt_in!=='boolean'||
       (!body.logs_opt_in && body.logs))return new Response('Invalid report',{status:400});
    // Two persistent independent throttles: stable installation identifier AND
    // address. Neither raw address nor secret email is stored in the client.
    const installBucket=await hmac(env.REPORT_HASH_KEY,`install:${body.client_id}`);
    const ipBucket=await hmac(env.REPORT_HASH_KEY,`ip:${ip}`);
    const installGate=env.REPORT_GATE.get(env.REPORT_GATE.idFromName(installBucket));
    const ipGate=env.REPORT_GATE.get(env.REPORT_GATE.idFromName(ipBucket));
    const acquire=async gate=>{
      const response=await gate.fetch('https://internal/gate',{method:'POST',body:JSON.stringify({action:'acquire'})});
      if(!response.ok)throw new Error('Rate gate unavailable');
      return response.json();
    };
    const release=async(gate,ticket)=>gate.fetch('https://internal/gate',{
      method:'POST',body:JSON.stringify({action:'release',receipt:ticket.receipt})});
    let installTicket,ipTicket;
    try{
      installTicket=await acquire(installGate);
      if(!installTicket.deliver)return text({received:true});
      ipTicket=await acquire(ipGate);
      if(!ipTicket.deliver){await release(installGate,installTicket);return text({received:true});}
    }catch{
      if(installTicket?.deliver)await release(installGate,installTicket);
      return new Response('Rate gate unavailable',{status:503});
    }
    const clean=s=>s.replace(/[\u0000-\u001f]/g,' ').slice(0,4500);
    const subject=clean(body.subject);
    const content=[`Version: ${body.version}`,`Subject: ${subject}`, '', clean(body.description),
      '', body.logs_opt_in?'Sanitized recent logs (user opt-in):':'No logs attached.',body.logs_opt_in?body.logs:''].join('\n');
    let delivered=false;
    try{
      const response=await fetch('https://api.resend.com/emails',{
        method:'POST',headers:{'Authorization':`Bearer ${env.RESEND_API_KEY}`,'Content-Type':'application/json'},
        body:JSON.stringify({from:env.REPORT_FROM_EMAIL,to:[env.REPORT_TO_EMAIL],
          subject:`[ProTanki bug] ${subject}`,text:content})
      });
      delivered=response.ok;
    }catch{}
    if(!delivered){
      await release(installGate,installTicket);
      await release(ipGate,ipTicket);
      return new Response('Delivery unavailable',{status:503});
    }
    return text({received:true});
  }
};
