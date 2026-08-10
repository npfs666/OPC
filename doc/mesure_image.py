#sudo dnf install python3-pillow-tk python3-tkinter
import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk
import math


class ImageMeasurementApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Mesure sur image")
        self.root.geometry("1200x800")

        # Image
        self.original_image = None
        self.display_image = None
        self.photo = None

        # Zoom
        self.zoom = 1.0
        self.min_zoom = 0.1
        self.max_zoom = 10.0

        # Points
        self.reference_points = []
        self.measurement_points = []

        # Échelle
        self.pixels_per_cm = None

        # Objets enregistrés
        self.measurements = []
        self.rectangles = []

        # Mode actuel :
        # None / reference / measurement / rectangle
        self.mode = None

        # Rectangle en cours
        self.rectangle_start = None

        self.create_ui()

    # =========================================================
    # Interface
    # =========================================================

    def create_ui(self):
        toolbar = tk.Frame(self.root)
        toolbar.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)

        tk.Button(
            toolbar,
            text="Ouvrir une image",
            command=self.open_image
        ).pack(side=tk.LEFT, padx=5)

        tk.Label(
            toolbar,
            text="Distance référence :"
        ).pack(side=tk.LEFT, padx=(20, 5))

        self.reference_distance_var = tk.StringVar(value="10")

        tk.Entry(
            toolbar,
            textvariable=self.reference_distance_var,
            width=8
        ).pack(side=tk.LEFT)

        tk.Label(
            toolbar,
            text="cm"
        ).pack(side=tk.LEFT, padx=5)

        tk.Button(
            toolbar,
            text="Définir l'échelle",
            command=self.start_reference
        ).pack(side=tk.LEFT, padx=10)

        tk.Button(
            toolbar,
            text="Mesurer distance",
            command=self.start_measurement
        ).pack(side=tk.LEFT, padx=5)

        tk.Button(
            toolbar,
            text="Rectangle",
            command=self.start_rectangle
        ).pack(side=tk.LEFT, padx=5)

        tk.Button(
            toolbar,
            text="Effacer mesures",
            command=self.clear_measurements
        ).pack(side=tk.LEFT, padx=5)

        # Affichage du zoom
        self.zoom_var = tk.StringVar(value="Zoom : 100 %")

        tk.Label(
            toolbar,
            textvariable=self.zoom_var
        ).pack(side=tk.RIGHT, padx=10)

        # Barre d'état
        self.status_var = tk.StringVar(
            value="Chargez une image."
        )

        status = tk.Label(
            self.root,
            textvariable=self.status_var,
            anchor="w"
        )
        status.pack(
            side=tk.BOTTOM,
            fill=tk.X,
            padx=5,
            pady=5
        )

        # =====================================================
        # Canvas
        # =====================================================

        frame = tk.Frame(self.root)
        frame.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(
            frame,
            background="gray20",
            cursor="cross"
        )

        x_scroll = tk.Scrollbar(
            frame,
            orient=tk.HORIZONTAL,
            command=self.canvas.xview
        )

        y_scroll = tk.Scrollbar(
            frame,
            orient=tk.VERTICAL,
            command=self.canvas.yview
        )

        self.canvas.configure(
            xscrollcommand=x_scroll.set,
            yscrollcommand=y_scroll.set
        )

        self.canvas.grid(
            row=0,
            column=0,
            sticky="nsew"
        )

        y_scroll.grid(
            row=0,
            column=1,
            sticky="ns"
        )

        x_scroll.grid(
            row=1,
            column=0,
            sticky="ew"
        )

        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)

        # Souris
        self.canvas.bind(
            "<ButtonPress-1>",
            self.mouse_press
        )

        self.canvas.bind(
            "<B1-Motion>",
            self.mouse_drag
        )

        self.canvas.bind(
            "<ButtonRelease-1>",
            self.mouse_release
        )

        # Molette Windows / macOS
        self.canvas.bind(
            "<MouseWheel>",
            self.mouse_wheel
        )

        # Molette Linux
        self.canvas.bind(
            "<Button-4>",
            self.mouse_wheel
        )

        self.canvas.bind(
            "<Button-5>",
            self.mouse_wheel
        )

    # =========================================================
    # Image
    # =========================================================

    def open_image(self):
        filename = filedialog.askopenfilename(
            filetypes=[
                (
                    "Images",
                    "*.png *.jpg *.jpeg *.bmp *.tif *.tiff"
                ),
                ("Tous les fichiers", "*.*")
            ]
        )

        if not filename:
            return

        self.original_image = Image.open(
            filename
        ).convert("RGB")

        self.zoom = 1.0
        self.zoom_var.set("Zoom : 100 %")

        self.reference_points.clear()
        self.measurement_points.clear()

        self.measurements.clear()
        self.rectangles.clear()

        self.pixels_per_cm = None
        self.mode = None

        self.update_display()

        self.status_var.set(
            "Image chargée. Définissez maintenant l'échelle."
        )

    # =========================================================
    # Zoom
    # =========================================================

    def mouse_wheel(self, event):
        if self.original_image is None:
            return

        # Position de la souris dans le canvas
        canvas_x = self.canvas.canvasx(event.x)
        canvas_y = self.canvas.canvasy(event.y)

        # Position correspondante dans l'image originale
        image_x = canvas_x / self.zoom
        image_y = canvas_y / self.zoom

        # Détermination du sens de la molette
        if event.num == 4:
            zoom_factor = 1.15

        elif event.num == 5:
            zoom_factor = 1 / 1.15

        elif event.delta > 0:
            zoom_factor = 1.15

        else:
            zoom_factor = 1 / 1.15

        new_zoom = self.zoom * zoom_factor

        new_zoom = max(
            self.min_zoom,
            min(self.max_zoom, new_zoom)
        )

        if new_zoom == self.zoom:
            return

        self.zoom = new_zoom

        self.zoom_var.set(
            f"Zoom : {self.zoom * 100:.0f} %"
        )

        # Redessiner
        self.update_display()

        # Nouvelle position du même point dans l'image zoomée
        new_x = image_x * self.zoom
        new_y = image_y * self.zoom

        image_width = (
            self.original_image.width * self.zoom
        )

        image_height = (
            self.original_image.height * self.zoom
        )

        # Décalage nécessaire pour garder le point
        # sous la souris
        scroll_x = new_x - event.x
        scroll_y = new_y - event.y

        if image_width > 0:
            self.canvas.xview_moveto(
                max(0, scroll_x / image_width)
            )

        if image_height > 0:
            self.canvas.yview_moveto(
                max(0, scroll_y / image_height)
            )

    # =========================================================
    # Affichage
    # =========================================================

    def update_display(self):
        if self.original_image is None:
            return

        width = max(
            1,
            int(self.original_image.width * self.zoom)
        )

        height = max(
            1,
            int(self.original_image.height * self.zoom)
        )

        self.display_image = self.original_image.resize(
            (width, height),
            Image.Resampling.LANCZOS
        )

        self.photo = ImageTk.PhotoImage(
            self.display_image
        )

        self.canvas.delete("all")

        self.canvas.create_image(
            0,
            0,
            image=self.photo,
            anchor=tk.NW
        )

        self.canvas.config(
            scrollregion=(0, 0, width, height)
        )

        self.redraw_measurements()
        self.redraw_rectangles()

    # =========================================================
    # Modes
    # =========================================================

    def start_reference(self):
        if self.original_image is None:
            messagebox.showwarning(
                "Attention",
                "Chargez d'abord une image."
            )
            return

        try:
            distance = float(
                self.reference_distance_var
                .get()
                .replace(",", ".")
            )

            if distance <= 0:
                raise ValueError

        except ValueError:
            messagebox.showerror(
                "Erreur",
                "Entrez une distance de référence valide."
            )
            return

        self.reference_points.clear()
        self.mode = "reference"

        self.status_var.set(
            f"Cliquez sur les 2 points correspondant à "
            f"{distance:g} cm."
        )

    def start_measurement(self):
        if self.pixels_per_cm is None:
            messagebox.showwarning(
                "Attention",
                "Vous devez d'abord définir l'échelle."
            )
            return

        self.measurement_points.clear()

        self.mode = "measurement"

        self.status_var.set(
            "Cliquez sur deux points pour mesurer une distance."
        )

    def start_rectangle(self):
        if self.pixels_per_cm is None:
            messagebox.showwarning(
                "Attention",
                "Vous devez d'abord définir l'échelle."
            )
            return

        self.mode = "rectangle"
        self.rectangle_start = None

        self.status_var.set(
            "Cliquez et faites glisser pour dessiner un rectangle."
        )

    # =========================================================
    # Souris
    # =========================================================

    def mouse_press(self, event):
        if self.original_image is None:
            return

        point = self.get_image_position(event)

        if point is None:
            return

        if self.mode == "reference":
            self.reference_points.append(point)

            self.draw_point(
                point[0] * self.zoom,
                point[1] * self.zoom,
                "yellow"
            )

            if len(self.reference_points) == 2:
                self.calculate_scale()

        elif self.mode == "measurement":
            self.measurement_points.append(point)

            self.draw_point(
                point[0] * self.zoom,
                point[1] * self.zoom,
                "red"
            )

            if len(self.measurement_points) == 2:
                self.calculate_measurement()

        elif self.mode == "rectangle":
            self.rectangle_start = point

    def mouse_drag(self, event):
        if (
            self.mode != "rectangle"
            or self.rectangle_start is None
        ):
            return

        current = self.get_image_position(event)

        if current is None:
            return

        self.draw_rectangle_preview(
            self.rectangle_start,
            current
        )

    def mouse_release(self, event):
        if (
            self.mode != "rectangle"
            or self.rectangle_start is None
        ):
            return

        end = self.get_image_position(event)

        if end is None:
            return

        x1, y1 = self.rectangle_start
        x2, y2 = end

        # Éviter les rectangles pratiquement nuls
        if (
            abs(x2 - x1) < 1
            or abs(y2 - y1) < 1
        ):
            self.canvas.delete("preview")
            self.rectangle_start = None
            return

        rectangle = {
            "p1": self.rectangle_start,
            "p2": end
        }

        self.rectangles.append(rectangle)

        self.rectangle_start = None

        self.update_display()

        width_cm = abs(x2 - x1) / self.pixels_per_cm
        height_cm = abs(y2 - y1) / self.pixels_per_cm

        center_x = (x1 + x2) / 2
        center_y = (y1 + y2) / 2

        self.status_var.set(
            f"Rectangle : "
            f"{width_cm:.2f} × {height_cm:.2f} cm | "
            f"Centre : ({center_x:.1f}, {center_y:.1f}) px"
        )

    # =========================================================
    # Coordonnées
    # =========================================================

    def get_image_position(self, event):
        canvas_x = self.canvas.canvasx(event.x)
        canvas_y = self.canvas.canvasy(event.y)

        image_x = canvas_x / self.zoom
        image_y = canvas_y / self.zoom

        # Limiter aux dimensions de l'image
        image_x = max(
            0,
            min(self.original_image.width, image_x)
        )

        image_y = max(
            0,
            min(self.original_image.height, image_y)
        )

        return image_x, image_y

    # =========================================================
    # Échelle
    # =========================================================

    def calculate_scale(self):
        p1, p2 = self.reference_points

        pixel_distance = self.distance(p1, p2)

        try:
            real_distance_cm = float(
                self.reference_distance_var
                .get()
                .replace(",", ".")
            )

        except ValueError:
            return

        self.pixels_per_cm = (
            pixel_distance / real_distance_cm
        )

        # Supprimer l'ancienne référence
        self.measurements = [
            m for m in self.measurements
            if m["type"] != "reference"
        ]

        self.measurements.insert(
            0,
            {
                "type": "reference",
                "p1": p1,
                "p2": p2,
                "distance": real_distance_cm
            }
        )

        self.mode = None

        self.update_display()

        self.status_var.set(
            f"Échelle : "
            f"{self.pixels_per_cm:.3f} pixels/cm"
        )

    # =========================================================
    # Mesure simple
    # =========================================================

    def calculate_measurement(self):
        p1, p2 = self.measurement_points

        pixel_distance = self.distance(p1, p2)

        distance_cm = (
            pixel_distance / self.pixels_per_cm
        )

        self.measurements.append(
            {
                "type": "measurement",
                "p1": p1,
                "p2": p2,
                "distance": distance_cm
            }
        )

        self.measurement_points.clear()

        self.update_display()

        self.status_var.set(
            f"Distance : {distance_cm:.2f} cm"
        )

        # Rester en mode mesure
        self.mode = "measurement"

    @staticmethod
    def distance(p1, p2):
        return math.hypot(
            p2[0] - p1[0],
            p2[1] - p1[1]
        )

    # =========================================================
    # Dessin points / lignes
    # =========================================================

    def draw_point(self, x, y, color):
        radius = 4

        self.canvas.create_oval(
            x - radius,
            y - radius,
            x + radius,
            y + radius,
            fill=color,
            outline="black",
            width=1
        )

    def redraw_measurements(self):
        for measurement in self.measurements:
            p1 = measurement["p1"]
            p2 = measurement["p2"]

            x1 = p1[0] * self.zoom
            y1 = p1[1] * self.zoom

            x2 = p2[0] * self.zoom
            y2 = p2[1] * self.zoom

            if measurement["type"] == "reference":
                color = "yellow"

                text = (
                    f'{measurement["distance"]:.2f} cm '
                    f'(réf.)'
                )

            else:
                color = "red"

                text = (
                    f'{measurement["distance"]:.2f} cm'
                )

            self.canvas.create_line(
                x1,
                y1,
                x2,
                y2,
                fill=color,
                width=2
            )

            radius = 4

            for x, y in ((x1, y1), (x2, y2)):
                self.canvas.create_oval(
                    x - radius,
                    y - radius,
                    x + radius,
                    y + radius,
                    fill=color,
                    outline="black"
                )

            mid_x = (x1 + x2) / 2
            mid_y = (y1 + y2) / 2

            self.canvas.create_text(
                mid_x,
                mid_y - 12,
                text=text,
                fill=color,
                font=("Arial", 12, "bold")
            )

    # =========================================================
    # Rectangle
    # =========================================================

    def draw_rectangle_preview(self, p1, p2):
        self.canvas.delete("preview")

        x1 = p1[0] * self.zoom
        y1 = p1[1] * self.zoom

        x2 = p2[0] * self.zoom
        y2 = p2[1] * self.zoom

        self.canvas.create_rectangle(
            x1,
            y1,
            x2,
            y2,
            outline="cyan",
            width=2,
            dash=(5, 3),
            tags="preview"
        )

        center_x = (x1 + x2) / 2
        center_y = (y1 + y2) / 2

        # Croix centrale
        size = 8

        self.canvas.create_line(
            center_x - size,
            center_y,
            center_x + size,
            center_y,
            fill="cyan",
            width=2,
            tags="preview"
        )

        self.canvas.create_line(
            center_x,
            center_y - size,
            center_x,
            center_y + size,
            fill="cyan",
            width=2,
            tags="preview"
        )

        if self.pixels_per_cm:
            width_cm = (
                abs(p2[0] - p1[0])
                / self.pixels_per_cm
            )

            height_cm = (
                abs(p2[1] - p1[1])
                / self.pixels_per_cm
            )

            text = (
                f"{width_cm:.2f} × "
                f"{height_cm:.2f} cm"
            )

            self.canvas.create_text(
                center_x,
                min(y1, y2) - 12,
                text=text,
                fill="cyan",
                font=("Arial", 12, "bold"),
                tags="preview"
            )

    def redraw_rectangles(self):
        if self.pixels_per_cm is None:
            return

        for rectangle in self.rectangles:
            p1 = rectangle["p1"]
            p2 = rectangle["p2"]

            image_x1, image_y1 = p1
            image_x2, image_y2 = p2

            x1 = image_x1 * self.zoom
            y1 = image_y1 * self.zoom

            x2 = image_x2 * self.zoom
            y2 = image_y2 * self.zoom

            width_cm = (
                abs(image_x2 - image_x1)
                / self.pixels_per_cm
            )

            height_cm = (
                abs(image_y2 - image_y1)
                / self.pixels_per_cm
            )

            # Centre dans l'image originale
            center_image_x = (
                image_x1 + image_x2
            ) / 2

            center_image_y = (
                image_y1 + image_y2
            ) / 2

            # Centre en cm depuis le coin supérieur gauche
            center_cm_x = (
                center_image_x / self.pixels_per_cm
            )

            center_cm_y = (
                center_image_y / self.pixels_per_cm
            )

            center_x = center_image_x * self.zoom
            center_y = center_image_y * self.zoom

            # Rectangle
            self.canvas.create_rectangle(
                x1,
                y1,
                x2,
                y2,
                outline="cyan",
                width=2
            )

            # Croix centrale
            cross_size = 8

            self.canvas.create_line(
                center_x - cross_size,
                center_y,
                center_x + cross_size,
                center_y,
                fill="cyan",
                width=2
            )

            self.canvas.create_line(
                center_x,
                center_y - cross_size,
                center_x,
                center_y + cross_size,
                fill="cyan",
                width=2
            )

            # Petit cercle au centre
            self.canvas.create_oval(
                center_x - 3,
                center_y - 3,
                center_x + 3,
                center_y + 3,
                fill="cyan",
                outline="black"
            )

            # Dimensions
            dimension_text = (
                f"{width_cm:.2f} × "
                f"{height_cm:.2f} cm"
            )

            self.canvas.create_text(
                (x1 + x2) / 2,
                min(y1, y2) - 13,
                text=dimension_text,
                fill="cyan",
                font=("Arial", 12, "bold")
            )

            # Informations centre
            center_text = (
                f"Centre : "
                f"{center_image_x:.1f}, "
                f"{center_image_y:.1f} px\n"
                f"{center_cm_x:.2f}, "
                f"{center_cm_y:.2f} cm"
            )

            self.canvas.create_text(
                center_x + 10,
                center_y + 10,
                text=center_text,
                fill="cyan",
                anchor=tk.NW,
                font=("Arial", 10, "bold")
            )

    # =========================================================
    # Effacement
    # =========================================================

    def clear_measurements(self):
        if self.original_image is None:
            return

        # Garder uniquement la référence
        self.measurements = [
            m for m in self.measurements
            if m["type"] == "reference"
        ]

        self.rectangles.clear()
        self.measurement_points.clear()

        self.rectangle_start = None
        self.mode = None

        self.canvas.delete("preview")

        self.update_display()

        if self.pixels_per_cm is not None:
            self.status_var.set(
                "Mesures et rectangles effacés. "
                "L'échelle est conservée."
            )


if __name__ == "__main__":
    root = tk.Tk()
    app = ImageMeasurementApp(root)
    root.mainloop()
