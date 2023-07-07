// Adds an entry to the event log on the page, optionally applying a specified
// CSS class.

let currentTransport;
let unistreamNumber;
let bistreamNumber;
let openbistreamNumber;
let currentTransportDatagramWriter;

// "Connect" button handler.
async function connect() {
  const url = document.getElementById('url').value;
  try {
    var transport = new WebTransport(url);
    transport.closed
    .then(() => {
      addToEventLog('Connection closed normally.', 'info', 'connection');
    })
    .catch(() => {
      addToEventLog('Connection closed abruptly.', 'error', 'connection');
    });
    addToEventLog('Initiating connection...', 'info', 'connection');
    try {
      await transport.ready;
      addToEventLog('Connection ready.', 'info', 'connection');
      afterCon(transport);
    } catch (e) {
      addToEventLog('Connection failed. ' + e, 'error', 'connection');
      return;
    }
  } catch (e) {
    addToEventLog('Failed to create connection object. ' + e, 'error', 'connection');
    return;
  }
}

function afterCon(transport) {
  currentTransport = transport;
  unistreamNumber = 1;
  bistreamNumber = 1;
  openbistreamNumber = 1;
  prepareDatagramWriter(transport);
  readDatagrams(transport);
  acceptUnidirectionalStreams(transport);
  acceptBidirectionalStreams(transport)
  document.forms.sendingdatagrams.elements.senddatagram.disabled = false;
  document.forms.sending.elements.send.disabled = false;
  document.getElementById('connect').disabled = true;
}

function prepareDatagramWriter(transport) {
  try {
    currentTransportDatagramWriter = transport.datagrams.writable.getWriter();
    addToEventLog('Datagram writer ready.', 'info', 'datagram-outgoin');
  } catch (e) {
    addToEventLog('Sending datagrams not supported: ' + e, 'error', 'datagram-outgoin');
    return;
  }

}

async function sendDatagram() {
  try {
    let form = document.forms.sendingdatagrams.elements;
    let rawData = form.datagram.value;
    let encoder = new TextEncoder('utf-8');
    let data = encoder.encode(rawData);
    await currentTransportDatagramWriter.write(data);
    addToEventLog('Sent datagram: ' + rawData, 'info', 'datagram-outgoin');
  } catch (e) {
    addToEventLog('Error while sending datagram: ' + e, 'error', 'datagram-outgoin');
  }
}

// "Send data" button handler.
async function sendData() {
  let form = document.forms.sending.elements;
  let encoder = new TextEncoder('utf-8');
  let rawData = sending.data.value;
  let data = encoder.encode(rawData);
  let transport = currentTransport;
  try {
    switch (form.sendtype.value) {
      case 'unidi': {
        const stream = await transport.createUnidirectionalStream();
        const writer = stream.getWriter();
        await writer.write(data);
        addToEventLog('Sent a unidirectional stream with data: ' + rawData, 'info', 'open-stream-uni');
        try {
          await writer.close();
          addToEventLog('stream closed normally.', 'info', 'open-stream-uni');
        } catch (error) {
          addToEventLog('stream closed error.', 'error', 'open-stream-uni');
        }
        break;
      }
      case 'bidi': {
        let stream = await transport.createBidirectionalStream();
        let number = openbistreamNumber++;
        readFromIncomingOpenStream(stream.readable, number);
        let writer = stream.writable.getWriter();
        await writer.write(data);
        await writer.close();
        addToEventLog('Opened bidirectional stream #' + number +' with data: ' + rawData, 'info', 'open-stream-bi');
        break;
      }
    }
  } catch (e) {
    addToEventLog('Error while sending data: ' + e, 'error', 'open-stream-uni');
  }
}

async function readFromIncomingOpenStream(readable, number) {
  let decoder = new TextDecoderStream('utf-8');
  let reader = readable.pipeThrough(decoder).getReader();
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) {
        addToEventLog('Stream #' + number + ' closed', 'info', 'open-stream-bi');
        break;
      }
      let data = value;
      addToEventLog('Received data on open bi stream #' + number + ': ' + data, 'info', 'open-stream-bi');
    }
  } catch (e) {
    addToEventLog('Error while reading from stream #' + number + ': ' + e + ' : ' + e.message, 'error', 'open-stream-bi');
  }
}

// Reads datagrams from |transport| into the event log until EOF is reached.
async function readDatagrams(transport) {
  try {
    var reader = transport.datagrams.readable.getReader();
    addToEventLog('Datagram reader ready.', 'info', 'datagram-incomming');
    let decoder = new TextDecoder('utf-8');
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) {
          addToEventLog('Done reading datagrams!', 'info', 'datagram-incomming');
          return;
        }
        let data = decoder.decode(value);
        addToEventLog('Datagram received: ' + data, 'info', 'datagram-incomming');
      }
    } catch (e) {
      addToEventLog('Error while reading datagrams: ' + e, 'error', 'datagram-incomming');
    }
  } catch (e) {
    addToEventLog('Receiving datagrams not supported: ' + e, 'error', 'datagram-incomming');
    return;
  }
}

async function acceptUnidirectionalStreams(transport) {
  try {
    const reader = transport.incomingUnidirectionalStreams.getReader();
    addToEventLog('uni stream reader ready.', 'info', 'stream-uni');
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) {
          addToEventLog('Done accepting unidirectional streams!', 'info', 'stream-uni');
          return;
        }
        let stream = value;
        let number = unistreamNumber++;
        addToEventLog('New incoming unidirectional stream #' + number, 'info', 'stream-uni');
        readFromIncomingUniStream(stream, number);
      }
    } catch (e) {
      addToEventLog('Error while accepting streams: ' + e, 'error', 'stream-uni');
    }
  } catch (e) {
    addToEventLog('Error unidirectional stream read: ' + e, 'error', 'stream-uni');
  }
}

async function readFromIncomingUniStream(stream, number) {
  let decoder = new TextDecoderStream('utf-8');
  let reader = stream.pipeThrough(decoder).getReader();
  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) {
        addToEventLog('Stream #' + number + ' closed', 'info', 'stream-uni');
        return;
      }
      let data = value;
      addToEventLog('Received data on uni stream #' + number + ': ' + data, 'info', 'stream-uni');
    }
  } catch (e) {
    addToEventLog('Error while reading from stream #' + number + ': ' + e + ' : ' + e.message, 'error', 'stream-uni');
  }
}

async function acceptBidirectionalStreams(transport) {
  try {
    const reader = transport.incomingBidirectionalStreams.getReader();
    addToEventLog('bi stream reader ready.', 'info', 'stream-bi');
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) {
          addToEventLog('Done accepting bidirectional streams!', 'info', 'stream-bi');
          return;
        }
        let stream = value;
        let number = bistreamNumber++;
        addToEventLog('New incoming biidirectional stream #' + number, 'info', 'stream-bi');
        readData(value.readable, number);
        writeData(value.writable, number);
      }
    } catch (e) {
      addToEventLog('Error while accepting streams: ' + e, 'error', 'stream-bi');
    }
  } catch (e) {
    addToEventLog('Error unidirectional stream reader: ' + e, 'error', 'stream-bi');
  }
}

async function readData(readable, number) {
  let decoder = new TextDecoderStream('utf-8');
  const reader = readable.pipeThrough(decoder).getReader();
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) {
        addToEventLog('Stream #' + number + ' closed', 'info', 'stream-bi');
        break;
      }
      let data = value;
      addToEventLog('Received data on bi stream #' + number + ': ' + data, 'info', 'stream-bi');
    }
  } catch (e) {
    addToEventLog('Error while reading from stream #' + number + ': ' + e + ' : ' + e.message, 'error', 'stream-bi');
  }
}

async function writeData(writable, number) {
  try {
    const writer = writable.getWriter();
    let encoder = new TextEncoder('utf-8');
    let rawData1 = 'hello ';
    let data1 = encoder.encode(rawData1);
    let rawData2 = 'world';
    let data2 = encoder.encode(rawData2);
    writer.write(data1);
    addToEventLog('write to incomming bidirectional stream #' + number +' with data: ' + rawData1, 'info', 'stream-bi');
    writer.write(data2);
    addToEventLog('write to incomming bidirectional stream #' + number +' with data: ' + rawData2, 'info', 'stream-bi');
  } catch (e) {
    addToEventLog('Error write to stream #' + number + ': ' + e + ' : ' + e.message, 'error', 'stream-bi');
  }
}

function addToEventLog(text, severity = 'info', id = 'event') {
  let log = document.getElementById(`${id}-log`);
  let mostRecentEntry = log.lastElementChild;
  let entry = document.createElement('li');
  entry.innerText = text;
  entry.className = 'log-' + severity;
  log.appendChild(entry);

  // If the most recent entry in the log was visible, scroll the log to the
  // newly added element.
  if (mostRecentEntry != null &&
      mostRecentEntry.getBoundingClientRect().top <
          log.getBoundingClientRect().bottom) {
    entry.scrollIntoView();
  }
}
