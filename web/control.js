// import io from 'socket.io-client'

// const socket = io('/control');

const ws = new WebSocket('ws://localhost:9000/control');
let settings;

ws.onopen =  () => {
    console.log("Connected")
    //   ws.send('something');
    setTimeout(() =>grabPreview(), 100);
};

ws.onmessage = (data) => {
    console.log(data)
    if(data.data instanceof Blob){
        const blob = new Blob([data.data], {type:'image/jpeg'}), 
        url = URL.createObjectURL(blob),
        img = document.getElementById('image')
        img.src = url;

        document.getElementById('overlay-img').src = url

        document.body.querySelector('#grab').innerText = "Grab preview"

        img.onload = () => {
            document.getElementById('resolution').innerText = `Resolution: ${img.naturalWidth}x${img.naturalHeight}`
        }

    } else if (typeof data.data == "string" ) {
        if(data.data.indexOf('setConfigComplete:') == 0) {
            setTimeout(() => grabPreview(), 500)   
        } else if(data.data.indexOf('settings:')== 0) {
            settings = JSON.parse(data.data.replace('settings:',''))
            console.log(settings)

            document.getElementById('iso').value = settings['camera']['iso']
            document.getElementById('exposureTime').value = settings['camera']['exposureTime']
            document.getElementById('opticalStabilization').checked = settings['camera']['opticalStabilization'] == "true" ? true : false
            document.getElementById('fastPreview').checked = settings['camera']['fastPreview'] == "true" ? true : false

        }
        // console.log(data.data)
    }
};

function setConfig(key, ...val) {
    ws.send('setConfig:'+key+':'+val.join(':'));
    console.log('setConfig:'+key+':'+val.join(':'))
}

function grabPreview() {
    document.body.querySelector('#grab').innerText = "Grab preview ⏳"
    ws.send('preview')

}
(async ()=>{
    // window.fetch('http://localhost:8000/actions/numPatterns', {
    //     mode: 'no-cors'
    // })
    // .then( res => res.text())
    // .then( res => console.log(res))


    document.body.querySelector('#grab').addEventListener('click', () => {
        grabPreview()
    })  

    document.body.querySelector('#start').addEventListener('click', () => {
        ws.send('start')
    })

    document.body.querySelector('#exposureTime').addEventListener('change', (ev) => {
        setConfig('exposureTime', parseFloat(ev.target.value));
    })

    document.body.querySelector('#iso').addEventListener('change', (ev) => {
        setConfig('iso', parseInt(ev.target.value));
    })
    document.body.querySelector('#opticalStabilization').addEventListener('change', (ev) => {
        setConfig('opticalStabilization', (ev.target.checked));
    })
    document.body.querySelector('#fastPreview').addEventListener('change', (ev) => {
        setConfig('fastPreview', (ev.target.checked));
        
        if(ev.target.checked) {
            document.body.querySelector('#iso').value = parseInt(document.body.querySelector('#iso').value) * 100
            document.body.querySelector('#exposureTime').value = parseFloat(document.body.querySelector('#exposureTime').value) / 100
        } else {
            document.body.querySelector('#iso').value = parseInt(document.body.querySelector('#iso').value) / 100          
            document.body.querySelector('#exposureTime').value = parseFloat(document.body.querySelector('#exposureTime').value) * 100          
        }
        setConfig('iso', parseInt(document.body.querySelector('#iso').value));
        setConfig('exposureTime', parseFloat(document.body.querySelector('#exposureTime').value));


    })

    document.body.querySelector("#image").addEventListener('click', (ev) => {
        let x = ev.target.naturalWidth * ev.offsetX / ev.target.width
        let y = ev.target.naturalHeight * ev.offsetY / ev.target.height
        setConfig('focus', parseInt(x), parseInt(y));

        // console.log(ev)
        // console.log(x,y)
    })

    document.getElementById('image').addEventListener('mousemove', (ev) => {
        const elm = document.getElementById('overlay');
        const img = document.getElementById('overlay-img')
        elm.style.display = 'block';
        elm.style.left = (ev.clientX - 150)+'px'
        elm.style.top = (ev.clientY + 50)+'px'

        img.style.position = 'relative';

        const scale = 2;
        let x = ev.target.naturalWidth * ev.offsetX / ev.target.width
        let y = ev.target.naturalHeight * ev.offsetY / ev.target.height
        
        img.style.left = (-(x - 150 / scale) * scale)+"px"
        img.style.top = (-(y - 150 / scale) * scale)+"px"

        img.style.width = (ev.target.naturalWidth*scale)+'px'

    })
    document.getElementById('image').addEventListener('mouseleave', (ev) => {
        document.getElementById('overlay').style.display = 'none';
    })

    document.body.onkeyup = function(e){
        if(e.keyCode == 32){
            grabPreview();
        }
    }
})();