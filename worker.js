import { parentPort } from 'worker_threads'
import { pynode } from "./index.js"

const longRunningFunction = () => {
  return new Promise((resolve, reject) => {
    console.log('cwd : ', process.cwd())

    pynode.startInterpreter()
    pynode.appendSysPath('./test_files')
    const performance = pynode.openFile('performance')
    
    performance.__getattr__('generate_slow_number').__callasync__(5, 7, (err, result) => {
      if (err) {
        reject(err)
        return
      }
      resolve(result)
    })
  })
}

longRunningFunction()
  .then(result => {
    console.log('result : ', result)
    parentPort.postMessage(result)
  })
  .catch(e => {
    console.log('err : ', e)
    parentPort.postMessage({error: e})
  })
