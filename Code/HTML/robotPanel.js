// RobotAdminPanel.js

export class RobotAdminPanel {
  constructor() {
    this.STATUS_POLL_MS = 9000;
    this.statusPollHandle = null;

    this.systemState = {
      emergency: false,
      motorsOn: false,
      gripperOn: false,
      remoteEnabled: true,
      absoluteMode: true,
      position: { x: 0, y: 0, z: 0 },
      connection: false
    };

    this.elements = {};
    this.sounds = this.initSounds();

    // por las dudas lo bindeamos si lo usás como callback
    this.refreshStatus = this.refreshStatus.bind(this);
  }

  // ---------- Inicialización de sonidos ----------

  initSounds() {
    const sounds = {
      emergency:   new Audio('./audios/parada_emergencia.mp3'),
      motorStart:  new Audio('./audios/arrancar_motor.mp3'),
      motorStop:   new Audio('./audios/detener_motor.mp3'),
      fueraRango:  new Audio('./audios/fuera_rango.mp3')
    };
    Object.values(sounds).forEach(a => { a.preload = 'auto'; a.volume = 0.9; });
    return sounds;
  }

  // ---------- Cacheo de elementos DOM ----------

  cacheElements() {
    this.elements = {
      log: document.getElementById('log'),
      queue: document.getElementById('queue'),
      files: document.getElementById('files'),
      tokenPreview: document.getElementById('tokenPreview'),
      usernamePreview: document.getElementById('usernamePreview'),
      privilegePreview: document.getElementById('privilegePreview'),
      connected: document.getElementById('connected'),
      mode: document.getElementById('mode'),
      motors: document.getElementById('motors'),
      grip: document.getElementById('grip'),
      posx: document.getElementById('posx'),
      posy: document.getElementById('posy'),
      posz: document.getElementById('posz'),
      ipPreview: document.getElementById('ipPreview'),
      lastUpdate: document.getElementById('lastUpdate'),
      remoteBadge: document.getElementById('remoteBadge')
    };
  }

  // ---------- Utilidades UI ----------

  logLine(message) {
    const timestamp = new Date().toLocaleTimeString();
    this.elements.log.textContent += `[${timestamp}] ${message}\n`;
    this.elements.log.scrollTop = this.elements.log.scrollHeight;
    this.elements.lastUpdate.textContent = new Date().toLocaleString();
  }

  updateBadge(element, value, type = 'neutral') {
    element.className = 'badge';
    if (type === 'on') element.classList.add('on');
    if (type === 'off') element.classList.add('off');
    if (type === 'warning') element.classList.add('warning');
    element.textContent = value;
  }

  enqueueCommand(command) {
    const item = document.createElement('div');
    item.textContent = `→ ${command}`;
    this.elements.queue.prepend(item);

    if (this.elements.queue.children.length > 15) {
      this.elements.queue.removeChild(this.elements.queue.lastChild);
    }
  }

  ensureRemoteAccess() {
    if (!this.systemState.remoteEnabled) {
      this.logLine('❌ Control remoto deshabilitado');
      return false;
    }
    return true;
  }

  // ---------- Parseo de respuesta XML de estado ----------

  parseEstadoResponse(xmlText) {
    try {
      const parser = new DOMParser();
      const doc = parser.parseFromString(xmlText, 'application/xml');
      if (doc.querySelector('parsererror')) throw new Error('XML inválido');

      const data = {};
      doc.querySelectorAll('member').forEach(member => {
        const name = member.querySelector('name');
        const value = member.querySelector('value');
        if (name && value) {
          data[name.textContent.trim()] = value.textContent.trim();
        }
      });

      return {
        x: parseFloat(data.x ?? '0'),
        y: parseFloat(data.y ?? '0'),
        z: parseFloat(data.z ?? '0'),
        modo: data.modo || 'ABS',
        motores: data.motores || 'OFF',
        garra: data.garra || 'OFF',
        emergencia: data.emergencia || 'NO',
        remoto: data.remoto === 'ON'
      };
    } catch (err) {
      console.error('No se pudo parsear getEstado', err);
      return null;
    }
  }

  // ---------- Networking / RPC ----------

  getServerIP() {
    if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
      return 'localhost';
    }
    return window.location.hostname;
  }

  async rpcCall(method, params = {}, options = {}) {
    const silent = !!options.silent;

    const serverIp = this.getServerIP();
    this.elements.ipPreview.textContent = serverIp;

    if (!serverIp) {
      if (!silent) this.logLine('❌ Error: No hay IP del servidor configurada');
      return null;
    }

    // Añadir información de sesión (usuario/token) automáticamente
    try {
      const tok = sessionStorage.getItem('auth_token');
      const user = sessionStorage.getItem('username');
      if (tok) params.token = tok;
      if (user) params.user = user;
    } catch (e) { /* ignore */ }

    const xmlBody = `<?xml version="1.0"?>
<methodCall>
  <methodName>${method}</methodName>
  <params>
    <param><value><string>${JSON.stringify(params)}</string></value></param>
  </params>
</methodCall>`;

    try {
      if (!silent) this.logLine(`📤 Enviando: ${method}`);
      const response = await fetch(`http://${serverIp}:8080`, {
        method: 'POST',
        headers: { 'Content-Type': 'text/xml' },
        body: xmlBody
      });

      const responseText = await response.text();

      if (responseText.includes('fault') || !response.ok) {
        throw new Error(`Error en RPC: ${responseText}`);
      }

      if (!silent) this.logLine(`✅ ${method} ejecutado correctamente`);
      return responseText;
    } catch (error) {
      if (!silent) this.logLine(`❌ Error en ${method}: ${error.message}`);
      return null;
    }
  }

  // ---------- Actualización de UI a partir de estado ----------

  updateMotorsUIState(newState) {
    this.systemState.motorsOn = newState;
    this.elements.motors.textContent = newState ? 'ON' : 'OFF';
    this.updateBadge(this.elements.motors, `Motores: ${newState ? 'ON' : 'OFF'}`, newState ? 'on' : 'off');
    document.getElementById('motorToggleBtn').textContent =
      newState ? '⚙️ Apagar Motores' : '⚙️ Encender Motores';
  }

  updateGripUIState(newState) {
    this.systemState.gripperOn = newState;
    this.elements.grip.textContent = newState ? 'ON' : 'OFF';
    this.updateBadge(this.elements.grip, `Garra: ${newState ? 'ON' : 'OFF'}`, newState ? 'on' : 'off');
    document.getElementById('gripToggleBtn').textContent =
      newState ? '🦾 Desactivar Garra' : '🦾 Activar Garra';
  }

  updateEmergencyUIState(active) {
    this.systemState.emergency = active;
    document.getElementById('estopBtn').style.display = active ? 'none' : 'inline-block';
    document.getElementById('resetBtn').style.display = active ? 'inline-block' : 'none';
    document.getElementById('emergencyBadge').style.display = active ? 'inline-block' : 'none';
    if (active) {
      this.elements.queue.innerHTML = '<div>🛑 PARADA DE EMERGENCIA - COLA LIMPIADA</div>';
    }
  }

  updateModeUI() {
    const isAbs = this.systemState.absoluteMode;
    this.updateBadge(this.elements.mode, `Modo: ${isAbs ? 'ABS' : 'REL'}`, isAbs ? 'on' : 'warning');
    document.getElementById('modeToggleBtn').textContent =
      isAbs ? '↗️ Cambiar a REL (G91)' : '📍 Cambiar a ABS (G90)';
  }

  updateRemoteUI(isEnabled) {
    this.systemState.remoteEnabled = isEnabled;
    this.updateBadge(
      this.elements.remoteBadge,
      `🌐 Control Remoto: ${isEnabled ? 'ON' : 'OFF'}`,
      isEnabled ? 'on' : 'off'
    );
    document.getElementById('remoteToggleBtn').textContent =
      isEnabled ? '🔒 Deshabilitar Control Remoto' : '🌐 Habilitar Control Remoto';
  }

  applyEstadoToUI(estado) {
    if (!estado) return;

    this.elements.posx.textContent = estado.x.toFixed(1);
    this.elements.posy.textContent = estado.y.toFixed(1);
    this.elements.posz.textContent = estado.z.toFixed(1);

    this.systemState.absoluteMode = estado.modo === 'ABS';
    this.updateModeUI();

    this.updateMotorsUIState(estado.motores === 'ON');
    this.updateGripUIState(estado.garra === 'ON');
    this.updateEmergencyUIState(estado.emergencia === 'SI');
    this.updateBadge(this.elements.connected, 'Conectado: OK', 'on');

    this.updateRemoteUI(estado.remoto);
  }

  // ---------- Control del robot ----------

  sendCommand(command) {
    if (this.systemState.emergency) {
      this.logLine('❌ Comando bloqueado - Sistema en emergencia');
      return;
    }
    if (!this.ensureRemoteAccess()) return;

    this.enqueueCommand(command);
    this.rpcCall(command, {});
  }

  moveRobot() {
    if (this.systemState.emergency) {
      this.logLine('❌ Movimiento bloqueado - Sistema en emergencia');
      return;
    }
    if (!this.ensureRemoteAccess()) return;

    const x = parseFloat(document.getElementById('x').value) || 0;
    const y = parseFloat(document.getElementById('y').value) || 0;
    const z = parseFloat(document.getElementById('z').value) || 0;
    const f = parseFloat(document.getElementById('f').value) || 1200;

    this.enqueueCommand(`G1 X${x} Y${y} Z${z} F${f}`);
    this.rpcCall('move', { x, y, z, f });

    this.elements.posx.textContent = x.toFixed(1);
    this.elements.posy.textContent = y.toFixed(1);
    this.elements.posz.textContent = z.toFixed(1);
  }

  toggleMotors() {
    const previousState = this.systemState.motorsOn;
    const newState = !previousState;
    if (newState && !this.ensureRemoteAccess()) return;

    this.updateMotorsUIState(newState);

    try {
      if (newState) {
        this.sounds.motorStart.currentTime = 0;
        this.sounds.motorStart.play().catch(() => {});
      } else {
        this.sounds.motorStop.currentTime = 0;
        this.sounds.motorStop.play().catch(() => {});
      }
    } catch (e) { /* ignore */ }

    this.enqueueCommand(newState ? 'M17' : 'M18');
    this.rpcCall('motors', { on: newState })
      .then(response => {
        if (!response) {
          this.logLine('⚠️ Error cambiando estado de motores, revirtiendo UI');
          this.updateMotorsUIState(previousState);
        }
      });
  }

  toggleGrip() {
    const previousState = this.systemState.gripperOn;
    const newState = !previousState;
    if (newState && !this.ensureRemoteAccess()) return;

    this.updateGripUIState(newState);

    this.enqueueCommand(newState ? 'M3' : 'M5');
    this.rpcCall('gripper', { on: newState })
      .then(response => {
        if (!response) {
          this.logLine('⚠️ Error cambiando estado de la garra, revirtiendo UI');
          this.updateGripUIState(previousState);
        }
      });
  }

  toggleMode() {
    if (this.systemState.emergency) {
      this.logLine('❌ Cambio de modo bloqueado - Sistema en emergencia');
      return;
    }
    if (!this.ensureRemoteAccess()) return;

    this.systemState.absoluteMode = !this.systemState.absoluteMode;
    const command = this.systemState.absoluteMode ? 'setAbs' : 'setRel';
    this.enqueueCommand(this.systemState.absoluteMode ? 'G90' : 'G91');
    this.rpcCall(command, {});
    this.updateModeUI();
  }

  toggleRemote() {
    const previousState = this.systemState.remoteEnabled;
    const desiredState = !previousState;
    this.updateRemoteUI(desiredState);
    this.logLine(desiredState ? '🌐 Control remoto habilitado' : '🔒 Control remoto deshabilitado');

    this.rpcCall(desiredState ? 'enableRemote' : 'disableRemote', {})
      .then(response => {
        if (!response) {
          this.logLine('⚠️ No se pudo actualizar el control remoto, revirtiendo estado');
          this.updateRemoteUI(previousState);
        }
      });
  }

  // ---------- Gestión de archivos ----------

  async uploadFile() {
    const fileInput = document.getElementById('file');
    if (!fileInput.files.length) {
      alert('Por favor selecciona un archivo');
      return;
    }

    const file = fileInput.files[0];

    const getServerIPLocal = () => {
      if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') return 'localhost';
      return window.location.hostname;
    };

    const serverIp = sessionStorage.getItem('server_ip') || getServerIPLocal();

    if (!serverIp) {
      alert('No hay servidor configurado');
      return;
    }

    const encodedName = encodeURIComponent(file.name);

    try {
      this.logLine(`📤 Subiendo archivo: ${file.name} a ${serverIp}`);
      const res = await fetch(`http://${serverIp}:8080/upload?name=${encodedName}`, {
        method: 'POST',
        headers: { 'Content-Type': 'text/csv' },
        body: file
      });

      const text = await res.text();
      if (!res.ok) {
        this.logLine(`❌ Error al subir: ${text}`);
        alert('Error al subir: ' + text);
        return;
      }

      const base = file.name.replace(/\.[^/.]+$/, '');
      const gcodePath = `jobs/${base}.gcode`;
      const item = document.createElement('div');
      item.textContent = gcodePath;
      item.onclick = () => this.selectFile(item);
      this.elements.files.appendChild(item);
      this.logLine(`✅ Subida completada: ${gcodePath}`);

      try {
        const gres = await fetch(`http://${serverIp}:8080/${gcodePath}`);
        if (gres.ok) {
          const gtext = await gres.text();
          this.logLine(`📁 EJECUTANDO ARCHIVO: ${gcodePath}`);
          const lines = gtext.split(/\r?\n/).map(l => l.trim()).filter(l => l.length > 0);
          for (const l of lines) {
            this.logLine(`➡️ Enviando: ${l}`);
            const resp = await this.rpcCall('sendGcode', { line: l });
            if (!resp) {
              this.logLine(`❌ Error al enviar línea: ${l}`);
              break;
            }
            await new Promise(r => setTimeout(r, 50));
          }
          this.logLine(`✅ Ejecución del archivo ${gcodePath} finalizada`);
        } else {
          this.logLine(`⚠️ No se pudo obtener ${gcodePath}: HTTP ${gres.status}`);
        }
      } catch (err) {
        this.logLine(`❌ Error leyendo ${gcodePath}: ${err.message}`);
      }
      fileInput.value = '';
    } catch (err) {
      console.error(err);
      this.logLine(`❌ Error al subir archivo: ${err.message}`);
      alert('Error al subir: ' + err.message);
    }
  }

  selectFile(element) {
    const previouslySelected = this.elements.files.querySelector('.selected');
    if (previouslySelected) previouslySelected.classList.remove('selected');
    element.classList.add('selected');
  }

  runSelectedFile() {
    const selected = this.elements.files.querySelector('.selected');
    if (!selected) {
      alert('Por favor selecciona un archivo de la lista');
      return;
    }

    let filePath = selected.textContent.trim();

    if (filePath.toLowerCase().endsWith('.csv')) {
      const base = filePath.replace(/\.csv$/i, '');
      const gpath = base + '.gcode';
      this.logLine(`⚠️ Se ha seleccionado un .csv; se usará ${gpath} para ejecución`);
      filePath = gpath;
    }

    this.enqueueCommand(`Ejecutar: ${filePath}`);
    this.rpcCall('runFile', { path: filePath });
  }

  refreshFileList() {
    this.logLine('🔄 Actualizando lista de archivos...');
    setTimeout(() => {
      this.logLine('✅ Lista de archivos actualizada');
    }, 1000);
  }

  // ---------- Sistema de aprendizaje ----------

  startLearning() {
    this.logLine('📚 Iniciando modo aprendizaje...');
    this.rpcCall('startLearning', {});
  }

  stopLearning() {
    this.logLine('📕 Deteniendo modo aprendizaje...');
    this.rpcCall('stopLearning', {});
  }

  // ---------- Control de emergencia ----------

  emergencyStop() {
    this.systemState.emergency = true;

    try {
      this.sounds.emergency.currentTime = 0;
      this.sounds.emergency.play().catch(() => {});
    } catch (e) { /* ignore */ }

    document.getElementById('estopBtn').style.display = 'none';
    document.getElementById('resetBtn').style.display = 'inline-block';
    document.getElementById('emergencyBadge').style.display = 'inline-block';

    this.elements.queue.innerHTML = '<div>🛑 PARADA DE EMERGENCIA - COLA LIMPIADA</div>';

    this.logLine('🛑 PARADA DE EMERGENCIA ACTIVADA');
    this.rpcCall('emergencyStop', { timestamp: new Date().toISOString() });
  }

  resetEmergency() {
    this.systemState.emergency = false;

    document.getElementById('estopBtn').style.display = 'inline-block';
    document.getElementById('resetBtn').style.display = 'none';
    document.getElementById('emergencyBadge').style.display = 'none';

    this.logLine('🔄 SISTEMA REINICIADO - Emergencia resetada');
    this.rpcCall('resetEmergency', {});
  }

  // ---------- Polling de estado ----------

  refreshStatus(options = {}) {
    const silent = !!options.silent;
    if (!silent) {
      this.logLine('🔄 Actualizando estado del sistema...');
    }
    this.rpcCall('getEstado', {}, { silent })
      .then(response => {
        if (response) {
          const estado = this.parseEstadoResponse(response);
          if (estado) {
            this.applyEstadoToUI(estado);
            if (!silent) this.logLine('✅ Estado del sistema actualizado');
          }
        }
      })
      .catch(err => {
        this.logLine(`❌ No se pudo actualizar estado: ${err.message}`);
      });
  }

  // ---------- Utilidades del sistema ----------

  clearLog() {
    if (confirm('¿Estás seguro de que quieres limpiar el log?')) {
      this.elements.log.textContent = '';
      this.logLine('🗑️ Log limpiado');
    }
  }

  exportLog() {
    const logContent = this.elements.log.textContent;
    const blob = new Blob([logContent], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `admin-log-${new Date().toISOString().split('T')[0]}.txt`;
    a.click();
    URL.revokeObjectURL(url);
    this.logLine('💾 Log exportado correctamente');
  }

  showUserInfo() {
    const info = `
👤 Información del Usuario
-------------------------
Usuario: ${sessionStorage.getItem('username') || 'No identificado'}
Privilegio: ${sessionStorage.getItem('user_privilege') || 'No definido'}
Servidor: ${sessionStorage.getItem('server_ip') || 'No configurado'}
Token: ${sessionStorage.getItem('auth_token') || 'No disponible'}
Sesión iniciada: ${new Date().toLocaleString()}
    `.trim();

    this.logLine(info);
  }

  // ---------- Inicialización general ----------

  initializePanel() {
    this.cacheElements();

    const serverIp = this.getServerIP();
    const username = sessionStorage.getItem('username');
    const privilege = sessionStorage.getItem('user_privilege');
    const token = sessionStorage.getItem('auth_token');

    this.elements.ipPreview.textContent = serverIp;
    this.elements.usernamePreview.textContent = username || '—';
    this.elements.privilegePreview.textContent = privilege || '—';
    this.elements.tokenPreview.textContent = token ? `...${token.slice(-4)}` : '—';

    this.updateBadge(this.elements.connected, 'Conectado: OK', 'on');
    this.updateBadge(this.elements.motors, 'Motores: OFF', 'off');
    this.updateBadge(this.elements.grip, 'Garra: OFF', 'off');
    this.updateModeUI();
    this.updateRemoteUI(this.systemState.remoteEnabled);

    this.logLine('🚀 Panel de administración inicializado');
    this.logLine(`👤 Usuario: ${username} (${privilege})`);
    this.logLine(`🌐 Servidor: ${serverIp}`);

    this.refreshStatus({ silent: true });
    this.statusPollHandle = setInterval(() => this.refreshStatus({ silent: true }), this.STATUS_POLL_MS);
    this.systemState.connection = true;
    this.logLine('✅ Conexión con el robot establecida');

    window.addEventListener('beforeunload', (e) => {
      if (this.systemState.motorsOn || this.systemState.gripperOn) {
        e.preventDefault();
        e.returnValue = 'El robot está en operación. ¿Estás seguro de que quieres salir?';
      }
    });
  }
}
