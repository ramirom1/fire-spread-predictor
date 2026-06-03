#!/usr/bin/env python3
"""
Script para generar gráficos de rendimiento a partir de resultados de experimentos
de simulación de propagación de incendios.

Genera 3 gráficos por cada tamaño de terreno:
  1. Escalabilidad (Tiempo vs. Procesos)
  2. Speedup
  3. Eficiencia

Los gráficos se guardan en la carpeta 'graficos/' como imágenes PNG.
"""

import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np

# ─────────────────────────────────────────────
# PARÁMETROS CONFIGURABLES
# ─────────────────────────────────────────────
TAMAÑOS_TERRENO = ["10000x10000", "15000x15000", "20000x20000"]
CSV_PATH = "resultados_experimentos.csv"
OUTPUT_DIR = "graficos"

# Paleta de colores profesional
COLORES = {1: "#2563eb", 3: "#e11d48", 5: "#16a34a"}
MARCADORES = {1: "o", 3: "s", 5: "D"}

# ─────────────────────────────────────────────
# LECTURA Y LIMPIEZA DE DATOS
# ─────────────────────────────────────────────
df = pd.read_csv(CSV_PATH)

# Manejo de datos faltantes: eliminar filas donde alguna columna de tiempo sea "Falta"
df = df[df["Tiempo (s)"] != "Falta"].copy()

# Convertir columnas numéricas (podrían ser string tras la lectura)
df["Tiempo (ms)"] = pd.to_numeric(df["Tiempo (ms)"])
df["Tiempo (s)"] = pd.to_numeric(df["Tiempo (s)"])
df["Procesos"] = df["Procesos"].astype(int)
df["Focos"] = df["Focos"].astype(int)

os.makedirs(OUTPUT_DIR, exist_ok=True)

# ─────────────────────────────────────────────
# CONFIGURACIÓN GLOBAL DE MATPLOTLIB
# ─────────────────────────────────────────────
plt.rcParams.update({
    "font.family": "sans-serif",
    "font.size": 11,
    "axes.titlesize": 14,
    "axes.titleweight": "bold",
    "axes.labelsize": 12,
    "legend.fontsize": 10,
    "figure.facecolor": "#f8fafc",
    "axes.facecolor": "#ffffff",
    "axes.edgecolor": "#cbd5e1",
    "grid.color": "#e2e8f0",
    "grid.linestyle": "--",
    "grid.alpha": 0.7,
})


def generar_graficos_para_tamaño(df_completo, tamaño):
    """Genera los 3 gráficos (combinado + individuales) para un tamaño de terreno."""

    # Filtrar por tamaño
    df_filtrado = df_completo[df_completo["Tamaño"] == tamaño].copy()

    if df_filtrado.empty:
        print(f"⚠️  No se encontraron datos para el tamaño '{tamaño}'. Saltando...")
        return

    # Valores únicos ordenados
    procesos_list = sorted(df_filtrado["Procesos"].unique())
    focos_list = sorted(df_filtrado["Focos"].unique())

    print(f"\n{'='*50}")
    print(f"Tamaño: {tamaño}")
    print(f"Procesos: {procesos_list}")
    print(f"Focos: {focos_list}")
    print(f"Filas utilizadas: {len(df_filtrado)}")

    # ── Cálculo de métricas ──
    resumen = (
        df_filtrado.groupby(["Focos", "Procesos"])["Tiempo (s)"]
        .mean()
        .reset_index()
        .rename(columns={"Tiempo (s)": "Tiempo_medio_s"})
    )

    # Speedup: S(p) = T(1) / T(p)
    tiempos_seq = resumen[resumen["Procesos"] == 1][["Focos", "Tiempo_medio_s"]].rename(
        columns={"Tiempo_medio_s": "T1"}
    )
    resumen = resumen.merge(tiempos_seq, on="Focos")
    resumen["Speedup"] = resumen["T1"] / resumen["Tiempo_medio_s"]

    # Eficiencia: E(p) = S(p) / p
    resumen["Eficiencia"] = resumen["Speedup"] / resumen["Procesos"]

    print(f"\nResumen de métricas:")
    print(resumen.to_string(index=False))

    max_p = max(procesos_list)

    # Etiqueta corta para el tamaño (para títulos)
    tamaño_corto = tamaño.replace("000x", "kx").replace("000", "k")  # "15000x15000" -> "15kx15k"

    # ─────────────────────────────────────────
    # GRÁFICO COMBINADO (1x3)
    # ─────────────────────────────────────────
    fig, axes = plt.subplots(1, 3, figsize=(20, 6))
    fig.suptitle(
        f"Análisis de Rendimiento — Terreno {tamaño}",
        fontsize=16, fontweight="bold", y=1.02
    )

    # ── 1. Escalabilidad (Tiempo vs. Procesos) ──
    ax1 = axes[0]
    for foco in focos_list:
        datos = resumen[resumen["Focos"] == foco]
        ax1.plot(
            datos["Procesos"], datos["Tiempo_medio_s"],
            marker=MARCADORES.get(foco, "o"),
            color=COLORES.get(foco, "#666"),
            linewidth=2.2, markersize=8,
            label=f"{foco} foco{'s' if foco > 1 else ''}",
            zorder=3
        )

    ax1.set_title("Escalabilidad")
    ax1.set_xlabel("Número de Procesos")
    ax1.set_ylabel("Tiempo (s)")
    ax1.set_xticks(procesos_list)
    ax1.legend(title="Focos", framealpha=0.9)
    ax1.grid(True)
    ax1.set_xlim(0, max_p + 0.5)
    ax1.set_ylim(bottom=0)

    # ── 2. Speedup ──
    ax2 = axes[1]

    # Línea de Speedup ideal (diagonal)
    max_speedup = resumen["Speedup"].max()
    ideal_limit = max(max_p, max_speedup)
    ideal_x = np.linspace(1, ideal_limit, 50)
    ax2.plot(
        ideal_x, ideal_x,
        linestyle="--", color="#94a3b8", linewidth=1.8,
        label="Speedup Ideal", zorder=2
    )

    for foco in focos_list:
        datos = resumen[resumen["Focos"] == foco]
        ax2.plot(
            datos["Procesos"], datos["Speedup"],
            marker=MARCADORES.get(foco, "o"),
            color=COLORES.get(foco, "#666"),
            linewidth=2.2, markersize=8,
            label=f"{foco} foco{'s' if foco > 1 else ''}",
            zorder=3
        )

    ax2.set_title("Speedup")
    ax2.set_xlabel("Número de Procesos")
    ax2.set_ylabel("Speedup (S = T₁ / Tₚ)")
    ax2.set_xticks(procesos_list)
    ax2.legend(title="Referencia / Focos", framealpha=0.9)
    ax2.grid(True)
    ax2.set_xlim(0, max_p + 0.5)
    ax2.set_ylim(bottom=0, top=max_speedup * 1.1)

    # ── 3. Eficiencia ──
    ax3 = axes[2]

    max_eficiencia = resumen["Eficiencia"].max()

    # Línea de eficiencia ideal (100%)
    ax3.axhline(
        y=1.0, linestyle="--", color="#94a3b8", linewidth=1.8,
        label="Eficiencia Ideal (100%)", zorder=2
    )

    for foco in focos_list:
        datos = resumen[resumen["Focos"] == foco]
        ax3.plot(
            datos["Procesos"], datos["Eficiencia"],
            marker=MARCADORES.get(foco, "o"),
            color=COLORES.get(foco, "#666"),
            linewidth=2.2, markersize=8,
            label=f"{foco} foco{'s' if foco > 1 else ''}",
            zorder=3
        )

    ax3.set_title("Eficiencia")
    ax3.set_xlabel("Número de Procesos")
    ax3.set_ylabel("Eficiencia (E = S / p)")
    ax3.set_xticks(procesos_list)
    ax3.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1.0))
    ax3.legend(title="Referencia / Focos", framealpha=0.9)
    ax3.grid(True)
    ax3.set_xlim(0, max_p + 0.5)
    ax3.set_ylim(0, max_eficiencia * 1.1)

    # ── Guardar gráfico combinado ──
    plt.tight_layout()
    ruta_combinada = os.path.join(OUTPUT_DIR, f"rendimiento_{tamaño}.png")
    fig.savefig(ruta_combinada, dpi=200, bbox_inches="tight", facecolor=fig.get_facecolor())
    print(f"\n✅ Gráfico combinado guardado en: {ruta_combinada}")

    # ── Guardar gráficos individuales ──
    titulos_archivos = ["escalabilidad", "speedup", "eficiencia"]
    for i, ax in enumerate(axes):
        fig_ind, ax_ind = plt.subplots(figsize=(8, 6))
        fig_ind.set_facecolor("#f8fafc")

        # Copiar las líneas del subplot original al nuevo
        for line in ax.get_lines():
            ax_ind.plot(
                line.get_xdata(), line.get_ydata(),
                linestyle=line.get_linestyle(),
                color=line.get_color(),
                linewidth=line.get_linewidth(),
                marker=line.get_marker(),
                markersize=line.get_markersize(),
                label=line.get_label(),
                zorder=line.get_zorder()
            )

        ax_ind.set_title(ax.get_title(), fontsize=14, fontweight="bold")
        ax_ind.set_xlabel(ax.get_xlabel())
        ax_ind.set_ylabel(ax.get_ylabel())
        ax_ind.set_xticks(procesos_list)
        ax_ind.set_xlim(ax.get_xlim())
        ax_ind.set_ylim(ax.get_ylim())
        ax_ind.legend(framealpha=0.9)
        ax_ind.grid(True, linestyle="--", alpha=0.7)

        if titulos_archivos[i] == "eficiencia":
            ax_ind.yaxis.set_major_formatter(ticker.PercentFormatter(xmax=1.0))

        ruta = os.path.join(OUTPUT_DIR, f"{titulos_archivos[i]}_{tamaño}.png")
        fig_ind.savefig(ruta, dpi=200, bbox_inches="tight", facecolor=fig_ind.get_facecolor())
        plt.close(fig_ind)
        print(f"✅ Gráfico individual guardado en: {ruta}")

    plt.close(fig)


# ─────────────────────────────────────────────
# EJECUCIÓN PRINCIPAL
# ─────────────────────────────────────────────
for tamaño in TAMAÑOS_TERRENO:
    generar_graficos_para_tamaño(df, tamaño)

print(f"\n🎉 ¡Todos los gráficos generados exitosamente en '{OUTPUT_DIR}/'!")
