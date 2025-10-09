"""
Interface de Linha de Comando do Servidor Central
Permite monitoramento e controle do estacionamento
"""

import asyncio
import sys
import os
from pathlib import Path
from datetime import datetime

sys.path.append(str(Path(__file__).parent.parent))

from comum.config import Config
from servidor_central.servidor_central import ServidorCentral

class InterfaceCLI:
    """Interface de linha de comando para o Servidor Central"""
    
    def __init__(self, servidor: ServidorCentral):
        self.servidor = servidor
        self.running = True
    
    def limpar_tela(self):
        """Limpa a tela do terminal"""
        os.system('clear' if os.name == 'posix' else 'cls')
    
    def exibir_status(self):
        """Exibe status do estacionamento"""
        self.limpar_tela()
        status = self.servidor.obter_status()
        
        print("=" * 80)
        print(" " * 25 + "SISTEMA DE ESTACIONAMENTO")
        print("=" * 80)
        print()
        
        # Horário
        print(f"Horário: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}")
        print()
        
        # Status geral
        print("STATUS GERAL:")
        print(f"  Estacionamento: {'FECHADO' if status['estacionamento_fechado'] else 'ABERTO'}")
        print(f"  Situação: {'LOTADO' if status['lotado'] else 'Disponível'}")
        print(f"  Veículos ativos: {status['veiculos_ativos']}")
        print()
        
        # Vagas por andar
        print("VAGAS DISPONÍVEIS:")
        print("-" * 80)
        
        andares = [
            ('Térreo', 'terreo'),
            ('1º Andar', 'andar1'),
            ('2º Andar', 'andar2')
        ]
        
        print(f"{'Andar':<15} {'PNE':<10} {'Idoso+':<10} {'Comuns':<10} {'Total':<10} {'Status'}")
        print("-" * 80)
        
        for nome, key in andares:
            vagas_livres = status['vagas_livres'][key]
            vagas_totais = status['vagas_totais'][key]
            
            pne = f"{vagas_livres['pne']}/{vagas_totais['pne']}"
            idoso = f"{vagas_livres['idoso']}/{vagas_totais['idoso']}"
            comuns = f"{vagas_livres['comuns']}/{vagas_totais['comuns']}"
            
            total_livre = sum(vagas_livres.values())
            total_total = sum(vagas_totais.values())
            total = f"{total_livre}/{total_total}"
            
            # Status
            if key == 'andar1' and status['andar1_bloqueado']:
                status_str = "BLOQUEADO"
            elif key == 'andar2' and status['andar2_bloqueado']:
                status_str = "BLOQUEADO"
            elif total_livre == 0:
                status_str = "LOTADO"
            else:
                status_str = "Disponível"
            
            print(f"{nome:<15} {pne:<10} {idoso:<10} {comuns:<10} {total:<10} {status_str}")
        
        print()
        
        # Número de carros por andar
        print("CARROS POR ANDAR:")
        print("-" * 80)
        print(f"  Térreo:   {status['num_carros']['terreo']} carros")
        print(f"  1º Andar: {status['num_carros']['andar1']} carros")
        print(f"  2º Andar: {status['num_carros']['andar2']} carros")
        print()
        
        # Menu de comandos
        print("=" * 80)
        print("COMANDOS:")
        print("  [1] Atualizar tela")
        print("  [2] Fechar/Abrir estacionamento")
        print("  [3] Bloquear/Desbloquear 1º andar")
        print("  [4] Bloquear/Desbloquear 2º andar")
        print("  [5] Ver histórico de saídas")
        print("  [6] Ver veículos ativos")
        print("  [0] Sair")
        print("=" * 80)
    
    async def processar_comando(self, comando: str):
        """Processa comando do usuário"""
        if comando == '1':
            # Atualiza tela
            pass
        
        elif comando == '2':
            # Fechar/Abrir estacionamento
            status = self.servidor.obter_status()
            novo_estado = not status['estacionamento_fechado']
            await self.servidor.fechar_estacionamento(novo_estado)
            acao = "fechado" if novo_estado else "aberto"
            print(f"\nEstacionamento {acao}!")
            await asyncio.sleep(1)
        
        elif comando == '3':
            # Bloquear/Desbloquear 1º andar
            status = self.servidor.obter_status()
            novo_estado = not status['andar1_bloqueado']
            await self.servidor.bloquear_andar(1, novo_estado)
            acao = "bloqueado" if novo_estado else "desbloqueado"
            print(f"\n1º andar {acao}!")
            await asyncio.sleep(1)
        
        elif comando == '4':
            # Bloquear/Desbloquear 2º andar
            status = self.servidor.obter_status()
            novo_estado = not status['andar2_bloqueado']
            await self.servidor.bloquear_andar(2, novo_estado)
            acao = "bloqueado" if novo_estado else "desbloqueado"
            print(f"\n2º andar {acao}!")
            await asyncio.sleep(1)
        
        elif comando == '5':
            # Ver histórico
            self.exibir_historico()
            input("\nPressione ENTER para continuar...")
        
        elif comando == '6':
            # Ver veículos ativos
            self.exibir_veiculos_ativos()
            input("\nPressione ENTER para continuar...")
        
        elif comando == '0':
            # Sair
            self.running = False
    
    def exibir_historico(self):
        """Exibe histórico de saídas"""
        self.limpar_tela()
        print("=" * 80)
        print(" " * 30 + "HISTÓRICO DE SAÍDAS")
        print("=" * 80)
        print()
        
        if not self.servidor.historico:
            print("Nenhum registro no histórico.")
            return
        
        # Últimos 10 registros
        registros = self.servidor.historico[-10:]
        
        print(f"{'Placa':<10} {'Entrada':<20} {'Saída':<20} {'Tempo':<10} {'Valor'}")
        print("-" * 80)
        
        for reg in registros:
            entrada = datetime.fromisoformat(reg.timestamp_entrada).strftime('%d/%m/%Y %H:%M:%S')
            saida = datetime.fromisoformat(reg.timestamp_saida).strftime('%d/%m/%Y %H:%M:%S')
            tempo = f"{reg.tempo_minutos}min"
            valor = f"R$ {reg.valor_pago:.2f}"
            
            print(f"{reg.placa:<10} {entrada:<20} {saida:<20} {tempo:<10} {valor}")
        
        print()
        print(f"Total de registros: {len(self.servidor.historico)}")
    
    def exibir_veiculos_ativos(self):
        """Exibe veículos atualmente no estacionamento"""
        self.limpar_tela()
        print("=" * 80)
        print(" " * 30 + "VEÍCULOS ATIVOS")
        print("=" * 80)
        print()
        
        if not self.servidor.veiculos_ativos:
            print("Nenhum veículo no estacionamento.")
            return
        
        print(f"{'Placa':<10} {'Entrada':<20} {'Andar':<10} {'Tempo':<10}")
        print("-" * 80)
        
        agora = datetime.now()
        for placa, reg in self.servidor.veiculos_ativos.items():
            entrada = datetime.fromisoformat(reg.timestamp_entrada).strftime('%d/%m/%Y %H:%M:%S')
            tempo_segundos = (agora - datetime.fromisoformat(reg.timestamp_entrada)).total_seconds()
            tempo_minutos = int(tempo_segundos / 60)
            tempo = f"{tempo_minutos}min"
            andar = reg.andar_atual.replace('andar', '').replace('terreo', 'Térreo')
            
            print(f"{placa:<10} {entrada:<20} {andar:<10} {tempo:<10}")
        
        print()
        print(f"Total de veículos: {len(self.servidor.veiculos_ativos)}")
    
    async def executar(self):
        """Executa loop da interface"""
        while self.running:
            self.exibir_status()
            
            # Lê comando (com timeout para atualização automática)
            try:
                # Usa asyncio para não bloquear
                comando = await asyncio.wait_for(
                    asyncio.get_event_loop().run_in_executor(
                        None, input, "\nComando: "
                    ),
                    timeout=5.0
                )
                await self.processar_comando(comando.strip())
            except asyncio.TimeoutError:
                # Atualiza tela automaticamente
                pass

async def main_com_interface():
    """Executa servidor com interface"""
    # Carrega configuração
    config = Config()
    
    # Cria servidor
    servidor = ServidorCentral(config)
    
    # Cria interface
    interface = InterfaceCLI(servidor)
    
    # Inicia servidor em background
    servidor_task = asyncio.create_task(servidor.iniciar())
    
    # Aguarda servidor iniciar
    await asyncio.sleep(3)
    
    try:
        # Executa interface
        await interface.executar()
    except KeyboardInterrupt:
        print("\n\nEncerrando...")
    finally:
        servidor_task.cancel()

if __name__ == "__main__":
    asyncio.run(main_com_interface())

