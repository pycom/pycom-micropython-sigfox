Pycom documentation contents
==================================

.. toctree::
	:numbered:
	
    pycom_esp32/getstarted
    pycom_esp32/toolsandfeatures
    pycom_esp32/tutorial/index
    library/index
    pycom_esp32/datasheets
    license

.. raw:: html

	
    <script type="text/javascript">
    function removeModules(elements,toRemove){
      el = document.querySelectorAll(elements)
      console.log("Removing from "+elements+" (found "+el.length+" items)")
      for(var i=0;i<el.length;i++){
          for(var j=0;j<toRemove.length;j++){
              key = toRemove[j]
              if(el[i].innerHTML.indexOf(key) > -1){
                  console.log(" > Found "+key+", removing now")
                  el[i].className += " hidden"
              }
          }
      }
    }

    var toRemove = ['Channel','BluetoothConnection','BluetoothService','BluetoothCharacteristic']
    
    removeModules('#pycom-modules blockquote div ul li',toRemove)

    removeModules('.toctree-l1.current ul .toctree-l2.current ul li',toRemove)
    </script>