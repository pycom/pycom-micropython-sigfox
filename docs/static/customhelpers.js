
function removeModules(elements,toRemove){
  el = document.querySelectorAll(elements)
  for(var i=0;i<el.length;i++){
      for(var j=0;j<toRemove.length;j++){
          key = toRemove[j]
          if(el[i].innerHTML.indexOf(key) > -1){
              el[i].className += " hidden"
          }
      }
  }
}
