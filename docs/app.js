const reduced=matchMedia('(prefers-reduced-motion: reduce)').matches;
const reveal=new IntersectionObserver(entries=>entries.forEach(entry=>{if(entry.isIntersecting){entry.target.classList.add('visible');reveal.unobserve(entry.target)}}),{threshold:.12});
document.querySelectorAll('.reveal,.reveal-scale').forEach(el=>reveal.observe(el));
const progress=document.querySelector('.progress i');
const shell=document.querySelector('.interface-shell');
function frame(){
  const max=document.documentElement.scrollHeight-innerHeight;
  if(progress)progress.style.transform=`scaleX(${max>0?scrollY/max:0})`;
  if(shell&&!reduced){
    const rect=shell.getBoundingClientRect();
    const center=rect.top+rect.height/2;
    const distance=Math.abs(center-innerHeight/2)/(innerHeight*.9);
    const scale=Math.max(.965,1-Math.min(distance,1)*.035);
    shell.style.setProperty('--scroll-scale',scale);
  }
}
addEventListener('scroll',frame,{passive:true});
addEventListener('resize',frame,{passive:true});
frame();